# Binary data

How Noeval could read and write arbitrary binary data, such as network packets
and binary file formats. This is a proposal: none of it is implemented.

## What Noeval has

Nothing built for bytes:

- No file or socket I/O. `load` evaluates a file, and `read` reads
  expressions from standard input.
- No bitwise operations on numbers.
- No compact storage for bytes. Strings are for text (see
  [strings.md](strings.md)), and the only other sequence is a list of
  numbers.

The closest thing is `codepoints->utf8` and `utf8->codepoints` in
`src/lib.noeval`, which work on lists of numbers and use `quotient` and
`remainder` where shifts and masks would be natural.

The proposal has five layers: storage, integer operations, decoding fields,
describing formats, and I/O.

## Storage: `:u8` vectors

[data-structures.md](data-structures.md) plans homogeneous vectors, as in
`(make-vector :u8 ...)`. Bytes should be that `:u8` case rather than a
separate `bytes` type. The planned representation suits binary data:

- **Slices are views** (block, offset, length), so taking a packet apart (the
  header, then the payload, then the next header) copies nothing.
- **RRB concatenation and slicing are what a streaming network parser does**:
  append each chunk as it arrives, and slice off the prefix it has consumed.
  This is a real workload where the RRB tree earns its complexity.

One part of that design doesn't fit: **leaf size is counted in elements
(32)**. For `:u8`, that's 32-byte leaves, so a 1,500-byte packet would be about
47 leaves and a 1 MB file about 32,000. Leaves with unboxed elements should be
sized in bytes, perhaps a few KB. That weakens the design's bound on what a
slice keeps alive (at most 31 elements outside its range): a 5-byte slice
could keep a whole 4 KB leaf alive. That seems an acceptable trade.

### Which element types binary data needs

Only `:u8`. Binary formats mix field widths, at unaligned offsets, with a
fixed byte order. Wider element types that reinterpret the same bytes, like
JavaScript's typed arrays over an `ArrayBuffer`, would expose the machine's
byte order to user code: the "efficiency features that leak implementation
details" that [musings.md](musings.md) warns about.

So binary formats use `:u8` plus decoding functions that take an explicit
byte order (see below). Whether to have `:u16`, `:s32`, `:f64` and so on is a
separate decision, about compact numeric arrays, and binary data doesn't need
to shape it.

### `map` and the element type

`map` over a `:u8` vector might return values that aren't bytes. Rather than
a rule that depends on the values, such as keeping `:u8` when every result
fits, the programmer should choose the type of the result. (A WebSocket
frame's XOR mask naturally wants bytes back, while a sum of pairs of bytes
doesn't.) Exactly how is left for later: an option such as
`(map f v :into :u8)` is one possibility.

### Printing

A vector prints as the call that constructs it: `(vector :u8 1 2 3)`. Every
other value that isn't a list, symbol, number or string prints now in an
unreadable `#<...>` form, but this form needs no reader extension, and
evaluating it gives an equal vector. It's what Smalltalk's `storeString` and
Racket's `print` do (see [printing.md](printing.md)).

Things to keep in mind:

- **It round-trips through `eval`, not through `read` alone.** Reading it
  gives a list whose first element is the symbol `vector`, which is equal to
  the vector only once it's evaluated, in an environment where `vector` means
  the library's constructor.
- **A vector and a list can print the same.** `(write v)` and
  `(write (list (q vector) :u8 1 2 3))` would both print
  `(vector :u8 1 2 3)`.
- **Only a vector at the top level evaluates back.** A list containing a
  vector prints as `(1 (vector :u8 2))`, and evaluating that is a call to 1.
  (Every value but a symbol or a cons cell evaluates to itself, so the vector
  itself could sit in the list, but its printed form can't.)

None of these matter for looking at a value at the REPL, which is what
printing is mostly for. If reader extensions arrive, a literal syntax can
replace this form.

## Integer operations: bitwise primitives

Masking and shifting with division costs O(bits) bignum operations per call,
so a library version is impractical, and these should be primitives. They're
cheap in C++: `bignum` is `cpp_rational`, whose numerator is a `cpp_int`,
with native bitwise operators.

- `bitwise-and`, `bitwise-or`, `bitwise-xor`, and `arithmetic-shift`, on
  integers as if in infinite two's complement, as in SRFI 151 and Common
  Lisp's `logand`. A number that isn't an integer is an error.
- `bitwise-not` can be `(- -1 x)` in the library.

They're useful without the rest of this proposal (the UTF-8 functions would
be simpler with them), so they could come first.

## Decoding fields

Converting between bytes and integers takes one more pair:
`(bytes->integer bytes :big :unsigned)` and
`(integer->bytes n width :little)`.

- As primitives, Boost's `import_bits` and `export_bits` do each conversion in
  one call, for any width (including 24, 48, or 128 bits) and either byte
  order.
- In the library, a fold, `(+ (* accumulator 256) byte)`, is fine for small
  widths.
- A signed value subtracts 2ⁿ when the top bit is set, in the library.

Since slices are views, `(bytes->integer (slice packet 4 8) :big)` copies
nothing.

### Floats

Every finite IEEE value is a dyadic rational, so an `f32` or `f64` field
decodes to an exact Noeval number with no rounding.

- NaN and the infinities have no Noeval number, and [numbers.md](numbers.md)
  rules infinity out, so decoding them raises an error or gives a keyword.
- Encoding has to be explicit about rounding: raise an error when a number
  isn't exactly representable, or take a rounding option.

## Describing formats: a bit-syntax macro

Erlang's bit syntax (`<<Version:4, IHL:4, TOS:8, Len:16/big, Rest/binary>>`)
is the best-known way to write binary layouts. Noeval could have one as a
library macro, with no interpreter support beyond the storage and the bitwise
primitives. Parsing an IPv4 header might look like:

```lisp
(binary-let packet
    ((version 4) (ihl 4) (dscp 6) (ecn 2) (total-length 16)
     (identification 16) (flags 3) (fragment-offset 13)
     (ttl 8) (protocol 8) (checksum 16)
     (source 32) (destination 32)
     (options (* 32 (- ihl 5))) (payload :rest))
  body...)
```

- The expansion is cached per combination (see [macros.md](macros.md)), so
  the fixed offsets are computed once, not once per packet.
- A field's size can be an expression over earlier fields, as with `options`.
- Fields smaller than a byte, and ones that cross byte boundaries, are where
  the bitwise primitives are essential.
- Big-endian (network order) is the default, with `:little` for a field.
- Bit order is an option too, since some formats, such as DEFLATE, read bits
  least significant first.

A matching `(binary (4 version) (4 ihl) ...)` would build bytes.

### Building bytes

For output, Erlang's iolists fit: `write-bytes` could accept a nested list of
byte vectors and write them with `writev`, so building a packet never has to
concatenate.

Updating a vector element by element is a trap. An update is in place only
when the vector is unshared, but in Noeval a binding in an enclosing
environment often still refers to the old version, so a loop of updates may
copy the vector every time. Builders that concatenate pieces or convert from
a list avoid that, and the library should encourage them.

## I/O

A handle is an opaque type. The minimum operations are:

- `open-file`
- `read-bytes`, which returns up to n bytes, possibly fewer, and the eof
  object at the end
- `write-bytes`
- `close`

The handle hides the operating system's object, so portable code doesn't
depend on it, but an accessor returns it (a file descriptor on POSIX, a
`HANDLE` on Windows) for code that wants to be specific to one operating
system. The handle still owns the object, and closes it when it's closed or
freed, so code that takes the object over needs a way to release it from the
handle as well.

Reference counting closes a handle deterministically, as soon as the last
reference to it goes, which most garbage-collected languages can't promise.
Still, explicit `close`, with `try`'s `finally`, should be the documented
practice.

Text I/O can be layered on top with UTF-8 decoding. Strings are stored as
UTF-8, so `string->utf8` and `utf8->string` would only copy, and they're
worth making primitives if converting through lists of codepoints is too
slow.

Sockets belong with the TODO's FFI and POSIX support. Byte vectors cause one
tension there: a short vector is contiguous and can be passed as a pointer,
but a long RRB tree has to be flattened or passed as an iovec.

## Order of work

1. The bitwise primitives. They're small and independent.
2. `:u8` vectors, possibly as the first homogeneous vector, and as flat arrays
   only at first, before the RRB tree exists.
3. File I/O on bytes, with handles.
4. The library's decoding functions (`bytes->integer`, floats), then the
   `binary-let` and `binary` macros.
5. Sockets, with the FFI and POSIX work.

## Open questions

- How the programmer chooses the element type of `map`'s result.
- Whether the bit-syntax macro's field specifications should be data (a
  layout that can be stored, passed, and reused) as well as syntax.
- How a handle releases its operating system object to code that takes it
  over.

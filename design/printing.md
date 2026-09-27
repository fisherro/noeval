# Printing

A survey of how other languages turn values into text and send text
somewhere, as groundwork for the TODO item about consolidating `write` and
`display`. The aim is broader than those two builtins: to choose primitives
that keep the language simple, let the library do more, and stay practical.

## What Noeval has

- `write` prints a value's printed form to standard output, and `display`
  does the same except that it prints a string's characters without quotes or
  escapes. Both return the value.
- `flush` flushes standard output. `read` reads an expression from standard
  input.
- `number->string` and `symbol->string` give a number's or a symbol's printed
  form as a string. `string->list` and `list->string` convert between a
  string and its codepoints.

So a value can only be printed to standard output. There's no way to get its
printed form as a string, to print to anywhere else, or to change how a kind
of value prints. The printed form isn't always readable: `(write
(string->symbol "a b"))` prints `a b`, an operative prints as its parameters
and body, and an environment prints as its address.

## Common Lisp

One function, `write`, does all the printing. It takes the object and
keyword arguments (`:stream`, `:escape`, `:readably`, `:base`, `:radix`,
`:circle`, `:length`, `:level`, `:pretty`, and others), each of which
defaults to a special (dynamic) variable: `*print-escape*`,
`*print-readably*`, `*print-base*`, and so on. The other printing functions
are `write` with some of those variables bound:

- `prin1` prints with escapes, for the reader (`*print-escape*` true).
- `princ` prints for people (`*print-escape*` and `*print-readably*` false).
- `print` is `prin1` preceded by a newline and followed by a space, and
  `pprint` prints with `*print-pretty*` true.
- `write-to-string`, `prin1-to-string`, and `princ-to-string` return the text
  instead of printing it.
- `format` is built on the same modes: `~S` prints as `prin1` does and `~A` as
  `princ` does. `(format nil ...)` returns a string.

Output goes to a stream, `*standard-output*` by default, which is itself a
special variable. `with-output-to-string` binds it to a string stream, so any
code that prints can be captured. There are also functions that write text
rather than values: `write-char`, `write-string`, `write-line`, `terpri`,
`fresh-line`, and `finish-output`/`force-output` to flush.

Printing is extensible through the `print-object` generic function, which
the printer calls for each object with the object and the stream. Methods
for user-defined classes print by writing to the stream, usually through the
other printing functions, and `print-unreadable-object` writes the
conventional `#<...>` form. With `*print-readably*` true, printing an object
that can't be read back signals an error instead of writing `#<...>`.

`*print-circle*` makes the printer label shared and circular structure
(`#1=(a . #1#)`), which the reader understands.

The standard doesn't let users define their own kinds of stream. The Gray
streams proposal, which most implementations support, does: a stream is a
class, and printing reaches it through generic functions such as
`stream-write-char` and `stream-write-string`.

## Scheme

R7RS has `write`, `display`, and two variants of `write`: `write-shared`
labels all shared structure, `write-simple` labels none (and may not
terminate on circular data), and `write` labels only what's needed to
terminate. `display` differs from `write` in printing strings and characters
as they are. Each takes an optional port.

Text is written with `write-char`, `write-string`, and `newline`, and bytes
with `write-u8` and `write-bytevector` on binary ports. `flush-output-port`
flushes. The current port is `current-output-port`, a parameter object (SRFI
39), so `(parameterize ((current-output-port p)) ...)` redirects output for
a dynamic extent. `open-output-string` and `get-output-string` make a string
port, and `open-input-string` reads from a string, so `read` can parse a
string. `with-output-to-file` and `call-with-output-file` open files.

R7RS has no `format` and no way to customize how a value prints. SRFI 28
adds a basic `format` (`~a`, `~s`, `~%`), and SRFI 48 a larger one.
Implementations add printing hooks for records: Guile's
`set-record-type-printer!` and Chez Scheme's `record-writer` take a
procedure of the record and a port. R6RS adds custom ports
(`make-custom-textual-output-port`), which take procedures to write, get and
set the position, and close.

### Racket

Racket has three modes: `write` (for the reader), `display` (for people),
and `print`, which the REPL uses and which prints a value as an expression
that produces it (so a list prints as `'(1 2)`). A struct customizes its
printing with `prop:custom-write` or the `gen:custom-write` interface, whose
procedure takes the value, a port, and the mode, so one procedure serves all
three. Parameters such as `print-graph` control the printer.

`format`, `printf`, and `fprintf` use `~a` (display), `~s` (write), and `~v`
(print). `with-output-to-string` captures output, and `make-output-port`
makes a port from procedures, so a user can define where output goes.

## Kernel

The Revised⁻¹ Report on Kernel (R-1RK) has ports as an optional module,
chapter 15. It enumerates `port?`, `input-port?`, `output-port?`,
`with-input-from-file`, `with-output-to-file`, `get-current-input-port`,
`get-current-output-port`, `open-input-file`, `open-output-file`,
`close-input-file`, `close-output-file`, `read`, and `write`, and in the
library part `call-with-input-file`, `call-with-output-file`, `load`, and
`get-module`. There's no `display`, no character or string output, and no
string ports, and klisp's documentation notes that the text of several of
those entries is missing from the report.

The design point that matters here is that the current ports are keyed
dynamic variables, Kernel's form of dynamic binding.
`make-keyed-dynamic-variable` returns a binder and an accessor. The binder
calls a combiner with the variable bound for the dynamic extent of the call,
and the accessor returns the current binding. `with-output-to-file` binds the
output port's variable this way, and `get-current-output-port` is its
accessor. So redirecting output isn't a special mechanism. It's the general
one, used for ports.

klisp, the most complete implementation, fills the gaps from R7RS: `display`,
`write-simple`, `write-char`, `newline`, string and bytevector ports,
`get-output-string`, and `flush-output-port`. Its `write` is guaranteed to
terminate on cyclic structure, which matters in Kernel, where pairs are
mutable and the report takes care to handle cyclic lists everywhere. Kernel
has user-defined types through encapsulations (`make-encapsulation-type`),
but no way to say how they print.

Sources: klisp's `src/kgports.c`, `doc/src/ports.texi`, and
`doc/src/keyed_vars.texi` ([GitHub mirror](https://github.com/dbohdan/klisp)).
The report itself is at WPI, which wasn't reachable when this was written.

## Clojure

Clojure also has two families of printing function, with separate names:

- `pr` and `prn` print for the reader, and `print` and `println` for people.
  (The `n` variants add a newline.) `pr` binds `*print-readably*` true, and
  `print` binds it false.
- `pr-str`, `prn-str`, `print-str`, and `println-str` return strings. They're
  built on `with-out-str`, which binds `*out*` to a `StringWriter`.
- `str` concatenates the display forms (Java's `toString`) of its arguments,
  and is the usual way to build a string from values.
- `format` is Java's `String.format`, and `clojure.pprint/cl-format` is a port
  of Common Lisp's `format`.

Output goes to `*out*` (and errors to `*err*`), a dynamic var bound with
`binding`. Printing is extensible through the `print-method` multimethod,
which dispatches on the value's type and takes the value and a writer.
`*print-length*` and `*print-level*` limit what's printed, `*print-meta*`
prints metadata, and `print-dup`/`*print-dup*` print values so that reading
them rebuilds the same types.

Tagged literals close the loop for user types: a `print-method` that writes
`#my/point [1 2]` and a reader function registered in `*data-readers*` make a
user type both printable and readable. `read-string` parses a string, and
`clojure.edn` reads the data subset safely.

## Other languages

- **Smalltalk**: every object has `printOn: aStream`, and that's the one
  method a class overrides. `printString` is defined once, in `Object`, by
  making a string stream and calling `printOn:`, and `displayString` and
  `displayNl` build on it. `storeOn:`/`storeString` write an expression that
  rebuilds the object, a third mode like Racket's `print`.
- **Python**: `repr` (for programmers) and `str` (for people) call the
  `__repr__` and `__str__` methods, and `format` calls `__format__`, which
  takes a format specification. `print` converts its arguments with `str` and
  writes to `sys.stdout`, or to any object with a `write` method passed as
  `file`. `io.StringIO` is a string stream, and
  `contextlib.redirect_stdout` rebinds `sys.stdout` for a block.
- **Rust**: two traits, `Display` (`{}`) and `Debug` (`{:?}`), each with one
  method that writes to a `Formatter`. `format!` returns a string, and
  `write!` writes to anything implementing `fmt::Write` or `io::Write`, so
  one implementation serves strings, files, and sockets.
- **Lua**: a small core. `io.write` writes only strings and numbers. Every
  other conversion goes through `tostring`, which calls a value's
  `__tostring` metamethod if it has one, and `print` is `tostring` on each
  argument followed by a write. `string.format` handles formatting.

## Patterns

Every language here separates the same few concerns, but divides them
differently between primitives and library.

### Two modes, sometimes three

All of them distinguish a form for the reader (`write`, `prin1`, `pr`,
`repr`, `Debug`) from a form for people (`display`, `princ`, `print`, `str`,
`Display`). Racket and Smalltalk add a third, an expression that rebuilds the
value. The mode is either a separate function (Scheme, Clojure, Python,
Rust) or an argument or dynamic variable of one function (Common Lisp's
`:escape`, Racket's mode argument to `custom-write`). In most, the modes
differ only for strings and characters, as `write` and `display` do in
Noeval.

### What's primitive: a string, or writing to a stream

There are two ways to layer printing:

- **Value to string is primitive** (Python, Lua, and in practice Clojure's
  `str`). Output needs only a primitive that writes a string. Everything else,
  including formatting, is string building. It's simple, but printing a large
  structure builds the whole string before writing any of it.
- **Writing to a stream is primitive** (Common Lisp, Scheme, Racket,
  Smalltalk, Rust). The "to string" versions write to a string stream. This
  needs a stream abstraction, but printing doesn't need an intermediate
  string, and user code can define new destinations.

Common Lisp, Racket, Smalltalk, and Rust all converge on printing to a
destination, with the string version defined once on top.

### Where output goes is dynamic

Common Lisp (`*standard-output*`), Scheme (`current-output-port`), Kernel
(a keyed dynamic variable), Clojure (`*out*`), and Python (`sys.stdout` with
`redirect_stdout`) all make the current output a dynamically bound variable.
That one mechanism gives capturing output as a string, redirecting it to a
file, and sending errors elsewhere, without adding a port argument to every
function that prints. Kernel is notable for not having a special mechanism:
ports use the same keyed dynamic variables as anything else.

### Customizing how a value prints

Where there are user-defined types, there's a hook: `print-object`,
`print-method`, `gen:custom-write`, `printOn:`, `__repr__`, `Display`,
`__tostring`. Almost always the hook takes the value and a destination and
writes to it, rather than returning a string, and the printer calls it for
each element as it walks a structure, so a user type inside a list prints
its own way. R7RS and Kernel have user-defined types (records,
encapsulations) but no standard hook.

### Printer controls

Common Lisp and Clojure have dynamic variables that limit or change what's
printed: length and depth limits, the radix for numbers, labels for shared
structure. Noeval's cons cells can't be modified, so a list can't be
circular and needs no labels, but length and depth limits would help the
REPL and error messages.

### Formatting builds on the modes

`format` in Common Lisp, SRFI 28, Racket, and Clojure's `cl-format` has
directives for the two modes (`~a` and `~s`) and writes to a destination or
returns a string. Given the primitives for the modes, it's library code.

### Reading mirrors writing

The reader side has the same split. Scheme's string ports, Common Lisp's
`read-from-string`, and Clojure's `read-string` let code parse a string
without going through a file or standard input. Clojure's tagged literals
extend reading the same way `print-method` extends printing.

## Questions for Noeval

These are for the design that follows, not decided here.

- **Which layer is primitive?** A primitive that returns a value's printed
  form as a string, plus one that writes a string, would let `write`,
  `display`, `newline`, `format`-style functions, and string building from
  values all live in the library. A printer that writes to a destination
  would need a port or stream type.
- **How much of the printer could be library code?** Numbers and symbols
  already have `number->string` and `symbol->string`, and lists and strings
  can be printed from their parts. Only operatives, macros, environments, and
  the eof object need the interpreter. A library printer would make a hook
  for user-defined types possible, but the REPL and error messages would
  still need the C++ printer, and it would be slower.
- **Should the destination be dynamic?** That needs dynamic variables, which
  are a separate TODO item. Kernel's keyed dynamic variables would serve
  ports, the test framework's counters, and printer controls alike. Without
  them, output functions could take an optional destination argument.
- **What should readable mean?** `write` doesn't currently guarantee that its
  output reads back (symbols with spaces or other delimiters, operatives,
  environments). Common Lisp's `*print-readably*` makes that a checkable
  promise. This also depends on the TODO item about escaping in string
  literals.
- **What about reading from strings?** A reader that takes a string would let
  the library parse without standard input, which the dependency checker and
  any self-hosting would need.

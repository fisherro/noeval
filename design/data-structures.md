# Data structures

Which data structures Noeval should build in, which should live in the
library, and what should represent sequences, including code. It starts from
first principles rather than from the existing C++ and Noeval code: the aim is
to pick the best structures and adapt the code to them.

## What Noeval has

- Cons cells, built with `cons` and taken apart with `first` and `rest`. Code
  is made of them, and each one carries a source location and a macro
  expansion cache. `cons` accepts any tail, so `(cons 1 2)` makes a pair that
  prints as `(1 . 2)`, but the reader has no dotted notation.
- Strings, as a separate type that converts to and from lists of codepoints.
- Environments, which are first-class and can be inspected, but can only map
  symbols, and are chained to their parents.
- Mutable bindings, created with `define-mutable`, which are the only mutable
  locations.

There are no vectors, bytevectors, records, hash tables, or user-defined types.
The TODO list asks about most of them.

## C's model versus Scheme's

C has only pointers, arrays, and structures, and there's an argument that this
fits R3RS's "removing the weaknesses and restrictions that make additional
features appear necessary" better than Scheme's list of types (pairs, vectors,
bytevectors, strings, records).

But C's three aren't really data structures. They describe a flat, mutable,
byte-addressed memory: where something is, how many there are, and how the
bytes are laid out. Everything else is left to convention, and the conventions
are where C's weaknesses are. C has no strings, only NUL-terminated arrays of
characters and pointer arithmetic over them. That minimalism creates a
weakness, which is the opposite of what R3RS asks for.

A garbage-collected language that's immutable by default can't adopt that
memory model, but its parts translate:

- **Pointers** do three jobs:
  1. Sharing, which every value in Noeval already has, since values are
     references.
  2. Referring to a mutable location, which a first-class box would do.
     `mutable_binding` is a box that can only be reached through a name.
  3. Arithmetic, to walk arrays, which indices or iteration replace.

  Only the third is unwanted, and the other two don't need it.
- **Arrays** become one indexed sequence type.
- **Structures** become a fixed group of fields plus a brand that marks it as
  a particular kind of thing.

Scheme's types mostly specialize those ideas: a pair is a two-field structure,
a record is a structure with a brand, a vector is an array, and strings and
bytevectors are arrays with compact storage for one element type. So the
question isn't C's route or Scheme's. It's which of these are semantic types
and which are only representations.

## What the sequence type has to do

- **Be persistent.** Noeval is immutable by default, so an update returns a
  new version and leaves the old one valid.
- **Be the only general sequence type, if possible.**
  [musings.md](musings.md) worries that overlapping types cost implementation
  complexity and cognitive load. That calls for a structure with no
  catastrophically slow operation, which doesn't need a second type to cover
  its weak spots.
- **Serve very different workloads:**
  - code: small forms, built by macros, taken apart with `first` and `rest`
  - data: iterate, append, index, update
  - text: concatenate, slice, search
  - stacks and accumulators: push and pop
- **Make use of reference counting.** See below.

## Candidates

The costs are for the persistent version of each structure.

| Structure | Cheap | Expensive |
|---|---|---|
| Cons list | `cons`, `first`, `rest`: O(1) | Index, length, append, concatenate: O(n). Poor locality. |
| Flat array | Index, iterate. The best locality. | Every persistent update, prepend, or concatenation copies: O(n) |
| Persistent trie (Clojure's vector) | Index, update, append: O(log₃₂ n) | Prepend, concatenate, slice: O(n) |
| Finger tree (Hinze and Paterson) | Both ends: O(1) amortized. Concatenate, split: O(log n) | Slower indexing, poor locality, large constant factors |
| RRB tree (Bagwell and Rompf) | Index, update, append, prepend, concatenate, slice: all O(log₃₂ n) | No O(n) operation. The costs are complexity and constant factors. |
| Flat array, copied when shared | As a flat array, and an unshared update is in place | Updating a shared array copies it: O(n) |

Linked lists suit modern hardware badly for most uses: a pointer to follow
for each element, the memory for each link, and O(n) indexing and length.
They're still good at what they're good at: `cons` is O(1) and shares the
whole tail, and `first` and `rest` are O(1) and allocate nothing.

Clojure is often cited as having moved to RRB trees, but its built-in vector
is the persistent trie. RRB trees generalize that trie to make concatenation
and slicing O(log n), and Clojure has them only in a contrib library,
[core.rrb-vector](https://github.com/clojure/core.rrb-vector). Clojure also
kept linked lists: code is made of them, `cons` makes one, and lazy seqs are
chains of cells. What changed was the default for data.

## Recommendation

### Decision: keep lists for now, and add vectors

After the microbenchmark and the discussion that followed it, the decision
is:

- **Lists stay for now**, made of cons cells, and code stays made of lists.
  This isn't permanent: lists may be replaced later. For now, the cons list is
  still the best structure for what code does most: taking a form apart and
  putting a new head on its operands.
- **Improper lists are gone**, as if lists had already been replaced (see
  [Improper lists](#improper-lists)). `cons`'s second argument must be a
  list.
- **`vector` is a new type**: a flat array up to 32 elements and an RRB tree
  above that, as described below.
- **A homogeneous vector** holds elements of one type, given to its
  constructor, as in `(make-vector :u8 ...)`. See
  [Homogeneous vectors](#homogeneous-vectors).
- **No literal syntax** for vectors for now. That waits for reader
  extensions.
- **Implementation waits** for a plan that breaks it into manageable pieces.

So there are two sequence types, as in Clojure, rather than one. That makes
the generic sequence interface (see [sequences.md](sequences.md)) more
important: library functions such as `map` and `foldl` should work on both,
through reduction.

### Homogeneous vectors

For now, assume a homogeneous vector can be made for any element type. How
to deal with arbitrary binary data, such as network packets and binary file
formats, is a later discussion, and it may shape what element types there
are.

"Any type" covers two different things, which the design needs to keep
apart:

- **Representations**, such as unsigned bytes or 32-bit integers. These
  aren't Noeval types (Noeval's numbers are all rationals), but they're what
  makes storage compact: the elements are stored unboxed, and leaves hold no
  references, so the cycle collector can skip them. Storing a value out of
  range, such as 256 in a vector of bytes, is an error.
- **Noeval types**, such as strings or symbols (and later, records). Storage
  gains nothing, since the elements are still values, so homogeneity is only
  a checked constraint.

Open question: whether homogeneous vectors are a distinct family of types
from heterogeneous ones. The alternative is one `vector` type whose element
type is a property of each vector, with the heterogeneous vector being the
one whose element type is "any". That keeps one set of vector functions and
one predicate. In Clojure, `vector-of` makes vectors of primitives that are
still vectors (`vector?` is true of them). One type does need rules for
operations whose results might not fit the element type: `map` over a vector
of bytes may return values that aren't bytes, and concatenating a vector of
bytes with a heterogeneous vector has to give a heterogeneous one.

The rest of this section is the analysis that led here, which argued for a
single sequence type.

### Sequences: an RRB tree, updated in place when unshared

The microbenchmark below supports this for sequences longer than a leaf (32
elements), but not for shorter ones, which should be exact-size flat arrays.
See [What the results change](#what-the-results-change).

The RRB tree is the only candidate with no bad operation. log₃₂ n is at most
7 levels for 2³² elements, so every operation is effectively constant. That
gives a uniform performance model: there's no slow operation to stumble into,
which is what makes a single sequence type defensible. (A list type with a
hidden mix of representations would be fast or slow depending on how a list
was built, which musings.md calls leaking implementation details.)

It covers each workload:

- **`cons`, `first`, and `rest`** become prepend and slice, each O(log n)
  rather than O(1).
- **Code:** nearly every form has fewer than 32 elements, so it's a single
  leaf, which is a small array. Taking it apart is cheap and its locality is
  ideal. (But a leaf should be exact-size: immer gives even a one-element
  vector a 32-slot leaf.)
- **Text:** RRB leaves of bytes make a rope, with cheap concatenation and
  slicing. That's the "primitive sequence type that serves the needs of
  strings" that musings.md asks for, and it could replace the separate string
  type, with the library giving a codepoint view of the bytes.
- **Compact storage:** leaves can store one element type (bytes, codepoints)
  unboxed. This is a representation, not a new type. It's also the TODO
  item's "homogenous RRB trees of specific types (like bytes)".

Reference counting makes one more thing possible. When a node's count is 1,
nothing else can see it, so an update can change it in place without
breaking persistence. Clojure needs explicit transients for that. Lean 4
("Counting Immutable Beans") and Koka (Perceus) get it automatically from
reference counting, and Noeval can too. A loop that builds a sequence then
runs at the speed of a mutable array, while the semantics stay persistent.

For C++, [immer](https://github.com/arximboldi/immer) has `flex_vector`, an
RRB tree, with configurable memory and reference counting policies. It's a
reference implementation, but Noeval will have its own (see
[Design of the representations](#design-of-the-representations)).

### Iteration: reduce first, `first` and `rest` second

With an RRB tree, recursion with `rest` makes an O(log n) slice for each step.
It stays correct but isn't the fast path. The generic sequence interface (see
[sequences.md](sequences.md)) should be built on reduction, where the
collection drives the loop, as in Clojure's `reduce` and transducers. The
library's list functions would move to it.

### Dictionaries: a CHAMP map, separate from environments

The same reasoning for maps leads to a hash array mapped trie, specifically
the CHAMP variant (Steindorfer and Vinju). It's persistent, its operations are
effectively constant, and it can update in place when unshared too.

Environments shouldn't be the general map. They can only map symbols, a
lookup falls through to the parent, `define` can't rebind a name, and every
environment is tracked by the cycle collector, whose runs are triggered by
creating environments (see [env-gc.md](env-gc.md)).

Nor should environments be built on the map internally. A frame is mutated in
place by `define` and never needs an old version, so persistence buys nothing,
and most frames are probably a call's few parameters, where a linear scan of a
small array beats any hash table. What would speed lookups up is interning
symbols, so a name compares and hashes as a pointer, and storing small frames
as flat arrays. The exception would be a feature that needs a snapshot of an
environment as a map, such as a module exporting its bindings: then sharing
the map would be O(1) where copying a hash table is O(n).

Each map has its own equality and hash hooks, given when it's created (with
keywords, as in `(make-map :equal f :hash g)`, say). Every map derived from it
by adding or removing entries keeps them. The defaults:

- **Equality: `=`, but false for arguments of different types**, rather than
  an error. `=` raises that error to catch mistakes in user code, but a map
  whose keys are a mix of, say, symbols and numbers compares keys of different
  types whenever they hash alike. It could be a primitive, or the library's
  `(and (= (typeof a) (typeof b)) (= a b))`, but the C++ has to implement it
  anyway for the fast path below, so exposing it costs little.
- **Hash: a hash of any `value`, written in C++ and exposed to user code.** It
  has to agree with the default equality: equal values hash alike. So a
  number's hash comes from its canonical numerator and denominator, a string's
  and a symbol's from their contents, and a sequence's from its elements'
  hashes, in order. That's O(n), but a sequence is immutable, so its hash can
  be computed once and cached. A map's hash has to combine its entries'
  hashes in a way that doesn't depend on their order, such as a sum.

Details to settle:

- **Reflexivity.** `(= first first)` and `(= map map)` are false: `=` on
  builtins and on operatives without tags is never true, so such a key could
  never be found. The default equality has to fall back to identity for types
  without structural equality, and their hash has to be by identity to match.
  That could be the rule for `=` itself.
- **The fast path.** Calling a Noeval operative for every hash and comparison
  would be slow. When a map's hooks are the defaults, which the C++ can tell by
  identity, it calls the C++ functions directly.
- **The hooks' contract.** Equality has to be an equivalence relation, and
  equal keys have to hash alike. Neither can be checked, and breaking them
  gives wrong lookups rather than errors, so the contract needs documenting.
  A hook that raises in the middle of an update is safe: the map is
  persistent, so the old version is untouched.
- **Operations on two maps.** Merging two maps, or comparing them with `=`,
  needs both to have the same hooks. Raising an error when they differ is the
  simplest rule. Then `=` on two maps compares keys with the maps' equality
  hook and values with `=`, so a map's own hash (for a map used as a key) has
  to hash its keys with its hash hook and its values with the default hash.
  Hashing keys with the default hash would break with a coarser hook: under
  case-insensitive keys, maps whose keys are `"A"` and `"a"` are equal but
  would hash differently.
- **Hash values are unspecified.** Hashes by identity differ between runs, and
  so would everything if the hash were seeded per process. User code can use
  them for its own hash tables, but shouldn't store them or depend on their
  values. A per-process seed would make that hard to depend on by accident.
- **Sets** would take the same hooks.

### Records: Kernel's encapsulation types

Kernel's `make-encapsulation-type` returns a constructor, a predicate, and an
accessor for a new brand that no other code can forge. That's the smallest
primitive that makes user-defined types possible. Records with named fields
can be a library layer over a brand and a sequence. Closures can imitate
records now, but they don't print, compare, or `typeof` as a distinct type,
and a brand fixes that.

### Mutation: boxes, if first-class mutable state is wanted

Exposing `mutable_binding` as a value (`box`, `unbox`, `set-box!`) gives the
useful half of a C pointer without the arithmetic. The TODO notes that
promises may need a mutable array element, and a box may be enough.

### Summary

Primitives: lists (proper only), `vector` (a flat array up to 32 elements and
an RRB tree above that), homogeneous vectors, a CHAMP map, encapsulation
types, and possibly boxes. Library: records, and the codepoint view of text.

## Consequences and open questions

- **Improper lists are gone, and won't be imitated.** See
  [Improper lists](#improper-lists).
- **Lazy and infinite sequences need their own type.** An RRB tree is finite
  and fully built, so streams need a separate type with `first` and `rest`,
  like Clojure's lazy seqs. That type is about computation, not storage, so it
  isn't an argument for keeping cons lists.
- **Cyclic structures stay impossible.** They already are, since a pair can't
  be mutated.
- **Value size is a separate problem.** `sizeof(value)` is 160 bytes (g++ 14),
  and each value also has a separately allocated control block, because
  `value::make` uses `new` rather than `make_shared`. A sequence of
  `value_ptr` fixes the locality of the spine, not of the elements, so shrinking
  `value` matters too.

## Improper lists

`(cons 1 2)` would have no RRB equivalent. Decision: drop improper lists
rather than imitate them. This is done, although lists remain cons cells:
`cons` requires a list as its second argument and raises an error otherwise,
as Clojure's requires a sequence.

### What they were used for

Very little. The reader has no dotted notation, and variadic parameters use a
single symbol rather than `(a . rest)`, so improper lists appeared only in:

- three tests: that `(list? (cons 1 2))` is true (a quirk of `list?`), that
  `(typeof (cons 1 2))` is `cons-cell`, and one that used `partialr` to cons
  onto a string
- the printer's `(1 . 2)` form

### What replaces each use

In other Lisps, improper lists do five jobs. Each has a replacement that
doesn't need them:

1. **Tuples**, such as association list entries: 2-element sequences, which
   an RRB tree stores as one small leaf, or records when the fields deserve
   names. `(rest p)` becomes `(second p)`.
2. **Rest parameters**, `(a b . rest)`: syntax in the parameter list, such as
   `(a b & rest)` as in Clojure, or a keyword.
3. **Destructuring patterns**, such as `syntax-rules`'s `(_ x . rest)` or
   Kernel's parameter trees, `((a . b) c)`: a pattern syntax of their own,
   such as `(_ x rest ...)` or `((a & b) c)`, wherever Noeval ends up needing a
   pattern language. Patterns don't need improper data to match against.
4. **Lazy streams**, which put a promise in the cdr: the separate lazy
   sequence type.
5. **Binary trees of conses**: 2-element sequences or records.

### Why not imitate them

Imitating them is possible: `(cons 1 2)` could make the sequence `[1]` with a
hidden tail of `2`, kept either as a sentinel element or in a tail field, and
`rest` would return the tail once the elements ran out. `cons`, `first`, and
`rest` would behave as they do now.

But every other sequence operation would need a rule for the tail: `length`,
`nth`, `reduce`, `map`, concatenation, slicing, equality, hashing, and
printing, plus the distinction between `list?` and `pair?`. Scheme makes most
of those an error on an improper list, so the sequence type would take on the
split between lists and pairs that it's meant not to have. A sentinel element
is the riskier form, since any builtin that forgets it exposes it to user
code, as array holes do in JavaScript. A tail field contains it better, but
costs a word and a branch in every operation.

That's a feature added because a restriction makes it appear necessary. The
restriction here is that a pair has to be a cons, and removing it is cheaper
than working around it.

### Migration

Done. `cons` raises an error for a second argument that isn't a list, the
printer no longer has a dotted form, and the tests that built improper lists
now build proper ones. (`utf8->codepoints`'s helper, whose comments wrote its
result as `(codepoint . remaining-bytes)`, turned out to return a proper list
whose rest is the remaining bytes, so it didn't change.)

## Microbenchmark

End-to-end Noeval benchmarks can't settle this, because the existing code is
shaped around cons cells. What can be checked independently is whether an RRB
tree's constant factors, especially on small sequences, are close enough to a
cons list's. That's the main risk in the recommendation.

### Structures

1. A cons list, with `std::shared_ptr` links, as Noeval has now.
2. A cons list with intrusive reference counts, to separate the cost of the
   structure from the cost of `shared_ptr`.
3. A flat array (`std::vector`), copied when shared and updated in place when
   not.
4. A persistent trie (`immer::vector`).
5. An RRB tree (`immer::flex_vector`).

The elements are intrusively reference-counted handles the size of a pointer,
so every structure pays for copying elements the way a real interpreter would.
The text workload also runs with unboxed `std::uint8_t` elements for the
structures that support them.

### Workloads

Each one runs at sizes 0, 1, 2, 4, 8, 16, 31, 32, 33, 1,000, and 1,000,000
elements, except where noted.

- **Build by appending** and **build by prepending**, keeping only the
  result, so the in-place path applies.
- **Build while keeping every version**, so nothing is unshared.
- **Walk with `first` and `rest`**, holding each intermediate value the way a
  recursive function's parameter would.
- **Reduce**, a sum using the structure's own iteration.
- **Index** at random positions, and **length**.
- **Update at an index**, both with the old version kept (shared) and dropped
  (unshared).
- **Concatenate** two sequences of the same size, and **slice** the middle
  half.
- **Code-shaped work**: many small sequences, with sizes drawn from the
  distribution of form lengths in `src/lib.noeval` and `tests/`. For each one,
  build it, take it apart as an evaluator would (`first`, then iterate the
  operands), and rebuild it as a macro would (a new head on the old operands).
- **Text-shaped work**: concatenate many short byte sequences into a long one,
  then slice it at random points.

### Measurements

- Instructions executed, counted with cachegrind, as the primary measure,
  since it doesn't depend on the machine's speed.
- Time, with the structures run alternately, as CONTRIBUTING.md describes for
  `benchmarks/run.bash -a`, to catch effects that instruction counts miss,
  such as cache misses. Cachegrind can also simulate caches for that.
- Bytes per element and allocations per operation, by counting in a custom
  allocator.

### Deciding in advance

Settle what would change the recommendation before running it:

- If the RRB tree is within a small factor (2×, say) of the cons list on the
  code-shaped workload and on the `first`-and-`rest` walk at small sizes, the
  recommendation stands.
- If the walk is much slower but reduce is fast, the recommendation stands,
  and the library should move to reduce, with `first` and `rest` for
  convenience only.
- If the code-shaped workload is much slower, consider representing a form as
  a single flat leaf (which it nearly always is anyway) with no tree around
  it, and check that again.
- If the flat array with in-place updates beats the RRB tree everywhere except
  shared updates, prepending, and concatenation, weigh how often those happen
  in real programs before choosing it.

### Results

The code and results are in
[experiments/sequences](../experiments/sequences/README.md), with every
number in `results.md`. These are nanoseconds per unit, the fastest of 21
runs, for the structures that allocate with malloc, and for the cons list
with a free list. (immer's RRB tree with its free lists is up to a third
faster than the one without, and mostly less, which changes none of the
comparisons.) Instruction counts, which don't
depend on the machine's speed, tell the same story.

| Workload | Size | cons | cons, free list | flat array | RRB tree |
|---|---:|---:|---:|---:|---:|
| build | 3 | 11.8 | 3.9 | 4.3 | 7.1 |
| walk with rest | 3 | 1.4 | 1.3 | 0.7 | 14.0 |
| walk with rest | 1,000,000 | 16.3 | 7.7 | 5.5 | 283 |
| reduce | 3 | 0.35 | 0.31 | 0.44 | 0.56 |
| reduce | 1,000,000 | 9.2 | 7.4 | 6.3 | 6.1 |
| reduce spine | 1,000,000 | 5.1 | 2.7 | 0.43 | 0.42 |
| prepend | 3 | 10.4 | 4.0 | 13.1 | 27.4 |
| prepend | 1,000,000 | 54 | 23 | (quadratic) | 1,148 |
| index | 1,000 | 613 | 543 | 0.75 | 1.6 |
| update, shared | 1,000 | 14,523 | 5,137 | 2,564 | 85 |
| concatenate | 1,000 | 29,204 | 14,225 | 5,056 | 398 |
| slice | 1,000 | 16,671 | 9,159 | 2.7 | 142 |
| code: parse (per form) | | 188 | 70 | 93 | 124 |
| code: evaluate (per form) | | 17.0 | 16.4 | 18.3 | 20.3 |
| code: expand (per form) | | 29.7 | 12.7 | 100 | 202 |
| text: concatenate (per byte) | | 24.3 | 12.1 | 0.50 | 0.97 |
| text: slice (per slice) | | 104,175 | 250,815 | 23 | 175 |

Bytes each structure allocates per element, not counting the elements'
objects:

| Size | cons | flat array | RRB tree |
|---:|---:|---:|---:|
| 1 | 24 | 40 | 264 |
| 3 | 24 | 18.7 | 88 |
| 32 | 24 | 8.75 | 8.25 |
| 1,000,000 | 24 | 8.0 | 8.5 |

Against the criteria set in advance:

- **The RRB tree isn't within 2× of the cons list on the code workload or on
  the walk at small sizes**, so the recommendation doesn't stand as it was.
  Evaluating forms is close (1.2×), and parsing them is faster than a cons
  list that allocates with malloc, but expanding them (a new head on the
  operands) is 7× to 16× slower, and a walk with `first` and `rest` of three
  elements is 10× slower. immer also gives even a one-element vector a full
  32-slot leaf, 264 bytes.
- **Reduction is fast**, as fast as a cons list's for small sequences and
  faster for large ones. So library code over sequences should reduce rather
  than recurse with `rest`: a `rest` of an RRB tree of a million elements
  takes about 280 ns, against 16 ns for a cons list.
- **The code workload is much slower, so the fallback was to represent a
  short sequence as a single flat leaf** and check again. A flat array in one
  allocation parses forms faster than the RRB tree and than a cons list that
  allocates with malloc, evaluates them about as fast as either, takes less
  memory than the cons list at every size from 2 and than the RRB tree at
  every size measured but 32, and walks with `rest` fastest of all, since
  `rest` is a view. It still expands forms 3× to 8× slower than a cons list:
  a cons list shares the operands, but an array copies them, and copying an
  element touches its reference count, in a scattered object (twice the
  cache misses per form).
- **Against the flat array, the RRB tree wins only where sequences are large
  and shared or combined**: shared updates, concatenation, prepending,
  persistent growth, and building one element at a time. The flat array wins
  at indexing, unique updates, slicing, and text.

Other findings:

- **immer's prepend is expensive**: 27 ns for three elements and 1.1 µs for a
  million, against 4 to 54 ns for a cons list. It goes through
  concatenation. Whether another RRB implementation would do better wasn't
  tested.
- **Free lists matter as much as the structure**: a free list makes the cons
  list 2× to 3× faster wherever it allocates. Any structure could have one.
  But it scatters a long list's nodes, since reused nodes aren't adjacent:
  slicing the long text is 2.4× slower with it.
- **The cons list's weaknesses are as expected**: at a thousand elements,
  indexing is 400 to 800 times slower than the RRB tree's and the flat
  array's, concatenation 3 to 70 times slower, and a slice 60 to 6,000 times
  slower.

### What the results change

One sequence type still looks right, but with two representations chosen by
size, which is what RRB implementations do in spirit (a short vector is a
single leaf):

- **Up to a leaf's size (32 elements), an exact-size flat array in one
  allocation.** Every operation on it is bounded by the size, so the
  performance model stays uniform.
- **Above that, an RRB tree**, with the library iterating by reduction.

The cost is expanding forms, the one workload where the cons list stays well
ahead. Macro expansions are cached, one per combination (see
[macros.md](macros.md)), so that cost is paid once per combination rather
than once per evaluation, while evaluation is the path that runs every time.

### Design of the representations

- **Every sequence handle is (block, offset, length).** Blocks are immutable
  and shared by reference count, so there's no separate kind of "view": a
  whole array is a handle with an offset of 0 and its block's length. `rest`
  and slices are new handles on the same block, with no copying.
- **A handle keeps its whole block alive**, including the elements outside
  its range. With blocks of at most 32 elements, that's at most 31 elements.
  (Java's `String.substring` shared its parent's array the same way until
  Java 7 update 6, when it switched to copying because small substrings kept
  large strings alive. The bound on block size avoids that here.)
- **A block is updated or grown in place** only when its handle is the only
  reference to it and covers all of it: a reference count of 1, an offset of
  0, and the block's full length. Otherwise the operation copies.
- **The flat array is the RRB tree's leaf type**, so a full block becomes the
  tree's first leaf without copying when a sequence grows past 32 elements:
  one new node on top, and the new element in a new leaf or tail.
- **A handle with a nonzero offset is copied when it becomes part of a
  tree**, rather than letting a tree refer to part of a leaf. That copies at
  most 32 elements, and keeps leaves simple.

Noeval will have its own implementation of both representations, rather than
using immer for the RRB tree:

- **The cycle collector has to see the nodes.** It visits each heap object
  once and subtracts the references that object holds (see
  [env-gc.md](env-gc.md)). In an RRB tree, the objects holding the elements
  are its nodes, which are shared between trees. Walking each tree's elements
  instead would subtract an element in a shared leaf once per tree, though
  the leaf holds only one reference to it, and the collector could free
  something still reachable. immer's nodes and their counts are in its
  `detail` namespace, not its public API.
- **It avoids a dependency** beyond Boost and Readline.
- **It lets a full flat array become a leaf without copying**, since the
  flat array can be the leaf type. (With immer, crossing 32 elements would
  copy them, which would also have been acceptable.)

When an RRB tree shrinks to 32 elements or fewer, through `rest`, a slice,
or a drop, it's copied back to a flat array, so short sequences are always
flat. If a sequence that keeps crossing the boundary ever copies too often,
demoting only below a lower size, such as 16, would fix it.

Two things are left to check:

- **A new handle needs a value in Noeval.** Every Noeval value is a separate
  allocation (351 instructions and 19 ns to allocate and free one of
  Noeval's size), so a `rest` that returns a new handle costs that too, where
  a cons list's `rest` returns a value that already exists. That's a
  question of how values are represented (see "Value size" above): it goes
  away if a sequence handle can be held in a value slot directly, rather
  than always behind a `value_ptr`.
- **The mixed representation itself** wasn't benchmarked: the numbers above
  are for the flat array and the RRB tree separately.

## References

- Phil Bagwell and Tiark Rompf, "RRB-Trees: Efficient Immutable Vectors",
  EPFL technical report, 2011.
- Nicolas Stucki, Tiark Rompf, Vlad Ureche, and Phil Bagwell, "RRB Vector: A
  Practical General Purpose Immutable Sequence", ICFP 2015.
- Ralf Hinze and Ross Paterson, "Finger trees: a simple general-purpose data
  structure", Journal of Functional Programming, 2006.
- Michael J. Steindorfer and Jurgen J. Vinju, "Optimizing Hash-Array Mapped
  Tries for Fast and Lean Immutable JVM Collections", OOPSLA 2015 (CHAMP).
- Sebastian Ullrich and Leonardo de Moura, "Counting Immutable Beans:
  Reference Counting Optimized for Purely Functional Programming", IFL 2019.
- Alex Reinking, Ningning Xie, Leonardo de Moura, and Daan Leijen, "Perceus:
  Garbage Free Reference Counting with Reuse", PLDI 2021.
- John N. Shutt, Revised⁻¹ Report on the Kernel Programming Language
  (encapsulation types).
- [immer](https://github.com/arximboldi/immer) and
  [core.rrb-vector](https://github.com/clojure/core.rrb-vector).

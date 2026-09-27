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

### Sequences: an RRB tree, updated in place when unshared

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
  ideal.
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
RRB tree, with configurable memory and reference counting policies. At the
least it's a reference implementation. It would also be a candidate
dependency, if one beyond Boost and Readline is acceptable.

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
creating environments (see [env-gc.md](env-gc.md)). Environment frames could
be built on the map internally, so the C++ still has one mechanism.

A map needs an equality and a hash for its keys, so this depends on the
questions in [equality.md](equality.md).

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

Primitives: an RRB sequence (with compact leaves for bytes and codepoints), a
CHAMP map, encapsulation types, and possibly boxes. Library: records, the
codepoint view of text, and bytevectors as sequences of bytes.

## Consequences and open questions

- **Improper lists go away.** `(cons 1 2)` has no RRB equivalent. Today the
  reader has no dotted notation, and variadic parameters use a single symbol,
  so improper lists appear only as pairs used as 2-tuples, as in
  `utf8->codepoints`'s `(codepoint . remaining-bytes)`, and in a test that
  `(list? (cons 1 2))` is true. Whether to imitate them is to be discussed.
- **Lazy and infinite sequences need their own type.** An RRB tree is finite
  and fully built, so streams need a separate type with `first` and `rest`,
  like Clojure's lazy seqs. That type is about computation, not storage, so it
  isn't an argument for keeping cons lists.
- **Cyclic structures stay impossible.** They already are, since a pair can't
  be mutated.
- **Per-combination metadata** (the source location and the expansion cache)
  moves from the cons cell to the node that holds the form.
- **Value size is a separate problem.** `sizeof(value)` is 160 bytes (g++ 14),
  and each value also has a separately allocated control block, because
  `value::make` uses `new` rather than `make_shared`. A sequence of
  `value_ptr` fixes the locality of the spine, not of the elements, so shrinking
  `value` matters too.

## Microbenchmark proposal

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

### Where it lives

A standalone C++ program in its own directory, such as
`experiments/sequences/`, outside the interpreter's build, with a Bash script
to build and run it. immer is header-only, so it only needs to be on the
include path for the experiment.

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

# Sequence microbenchmark

Compares persistent sequence structures on the operations Noeval would use
them for, to check the recommendation in
[design/data-structures.md](../../design/data-structures.md) of an RRB tree as
the one sequence type. It doesn't use the interpreter: the question is which
structure to adapt the interpreter to, not how the current code performs.

`run.bash` fetches [immer](https://github.com/arximboldi/immer) at a fixed
commit into `build/` (which git ignores), builds `bench.cpp`, runs it, and
writes `results.tsv` and `results.md`. It needs g++ 14 (or `CXX`), valgrind,
and the valgrind headers. It takes about ten minutes.

`forms.noeval` measures the lengths of the lists in the library and tests,
which the code workload uses:

```bash
cat src/lib.noeval tests/*.noeval |
    bin/noeval --skip-tests experiments/sequences/forms.noeval |
    grep -E '^[0-9]+$' | sort -n | uniq -c
```

## Structures

- **cons (shared_ptr)**: a cons list whose links are `std::shared_ptr`, each
  with a separate control block, as Noeval's cons cells are. Its reference
  counts are atomic, as `shared_ptr`'s are.
- **cons (intrusive)**: a cons list with intrusive, non-atomic reference
  counts. This is the fair baseline for a cons list.
- **flat array**: a `std::vector`, shared by reference count. An update is in
  place when nothing else refers to it, and a copy otherwise. `rest` and
  slices are views into it, so they allocate nothing, but they keep the whole
  array alive.
- **trie**: `immer::vector`, a persistent trie with 32-way branching, as
  Clojure's vector. It has no cheap `rest`, prepend, concatenation, or slice,
  so those copy.
- **rrb**: `immer::flex_vector`, an RRB tree.

immer is built with `IMMER_NO_THREAD_SAFETY`, so its counts aren't atomic
either, and it uses its free lists.

Every structure holds *elements*, which stand for Noeval's `value_ptr`: a
pointer to a separately allocated, reference-counted object. Copying an
element touches its object's count. The objects are allocated and then
shuffled, so neighbouring elements point to scattered objects, as they would
in a real heap. The text workloads hold bytes instead.

## Workloads

Sizes are 1, 2, 3, 4, 8, 16, 32, 33, 1,000, and 1,000,000. Small sizes are
repeated so each measurement covers at least 65,536 elements. A workload that
would be quadratic for a structure isn't run above 1,000 for it.

| Workload | Unit | What it does |
|---|---|---|
| build | element | Build from an array, the best way the structure allows (a cons list conses from the end) |
| build incrementally | element | Add elements in order, one at a time (a cons list conses onto the front and reverses) |
| prepend | element | Add each element at the front |
| persistent growth | element | Add each element at the structure's natural end (the front of a cons list, the back of the others) while the previous version is still referenced |
| walk with rest | element | Sum the elements with `first` and `rest`, holding each step's sequence as a recursive function's parameter would |
| reduce | element | Sum the elements' numbers with the structure's own iteration |
| reduce spine | element | Sum the elements' addresses, without reading their objects, to isolate the structure's own locality |
| length | call | Get the length |
| index | lookup | 10,000 lookups at random indices |
| update shared | update | 1,000 updates at random indices, each of the original sequence, which stays referenced |
| update unique | update | 1,000 updates at random indices, each of the previous result, the only reference to it |
| concatenate | call | Concatenate two sequences of the size, keeping both |
| slice | call | Take the middle half |
| code: parse | form | Build 11,528 forms with the lengths of the library's and tests' lists |
| code: evaluate | form | For each form, read its first element and then its operands in order |
| code: expand | form | For each form, put a new head on its operands (`rest`), as a macro would |
| text: concatenate | byte | Concatenate 5,000 byte strings of 1 to 40 bytes |
| text: slice | slice | Take 200 slices of 10 to 100 bytes at random from the result, and sum each |

The measurements include freeing what a workload creates and drops, but not
what it was given.

## Measures

- **Instructions**, counted by callgrind, which doesn't depend on the
  machine's speed.
- **First-level data cache misses**, as callgrind simulates them. The
  simulated last-level cache is the host's, which on a large server holds
  everything, so last-level misses aren't reported.
- **Allocations**, counted by replacing `operator new`.
- **Nanoseconds**, the fastest of 7 runs in each of 3 rounds. The machine's
  speed varies, so treat these as rough, and prefer the instruction counts
  for small differences.
- **Bytes per element** that the structure allocates, not counting the
  elements' objects, as `malloc_usable_size` reports them.

## Caveats

- In Noeval, every value is a separately allocated object, so an operation
  that returns a new sequence also allocates a value to hold it. `rest` on a
  cons list returns a value that already exists, but on every other
  structure it would allocate one. The baseline row in the results gives
  that cost, which isn't included in the structures' numbers.
- The cons list's `index`, `update`, `concatenate`, and `slice` are written
  in C++ over the nodes. In Noeval, they'd be library code over `first` and
  `rest`, and much slower.

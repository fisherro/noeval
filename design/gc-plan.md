# Environment GC plan

This plan fixes environment garbage collection while keeping fexprs and
first-class environments. It follows the recommendation in
[gc-postmortem.md](gc-postmortem.md).

## Goal

Reliably collect cyclic garbage, including in the middle of an evaluation,
without the C++ code having to register roots by hand.

## Approach: trial deletion

Every cycle in Noeval passes through an environment. Cons cells can't form
cycles, and `eval_symbol` unwraps mutable bindings, so Noeval code never gets
hold of a `mutable_binding` itself.

A trial-deletion collector (Bacon–Rajan; CPython's `gc` works this way) works
out the roots by itself:

1. **Count.** Walk every registered environment and every value reachable from
   one. Set each node's count to its `use_count()`, then subtract one for each
   reference to it found inside the heap:
   * environments: `parent`, `operative::closure_env`, `env_ptr` values
   * values: bindings, `car`/`cdr`, `mutable_binding::value`, `operative::body`
2. **Mark.** A node whose count is still above zero is referenced from outside
   the heap (the C++ stack, a `tail_call`, the REPL's top-level environment,
   ...). Those nodes are the roots. Mark everything reachable from them.
3. **Break.** Unmarked environments are cyclic garbage. Hold strong references
   to them in a vector, clear their bindings and `parent`, then release the
   vector.

When this goes wrong, it goes wrong safely. A reference the scan misses makes
its target look externally held, so the target is kept: a leak, not a
corrupted environment. The current design fails the other way: a missed root
has its bindings cleared.

## Phase 0: Baseline and failing tests

* Remove leftover debugging: the `std::println("{}({})", __FILE__, __LINE__)`
  lines and `get_debug().enable_all()` in `main`.
* Add GC tests to `src/tests.cpp`. Each one runs, collects, and asserts that the
  registered environment count returns to baseline:
  * a local helper defined inside a `vau` body, called 1,000 times
  * a `define-mutable` + `set!` cycle
  * an environment bound to itself via `get-current-environment`
  * an operative held only by a C++ temporary during a collection, e.g.
    `((wrap (vau (x) () x)) 1)`, with collection forced mid-evaluation

  With the current collector these fail, either by leaking or with
  `Unbound variable`.

## Phase 1: New collector

* Give `environment` `enable_shared_from_this` (`value` already has it). Read
  counts with `weak_from_this().use_count()`, because `lock()` would add a
  count of its own.
* Change the registry to a set of raw `environment*`, maintained by the
  constructor and destructor. Iterate over a snapshot, and don't destroy
  anything until the break step. This also covers the
  `environment::unregister` item in `TODO.md`.
* Rewrite `environment::collect()` as count → mark → break. Visit each value
  exactly once (keep a set of visited pointers) so that each `shared_ptr`
  member is subtracted exactly once.

## Phase 2: Delete the root machinery

* Delete `env_root_ptr`, the `roots` map, `add_root`/`remove_root`,
  `dump_roots`, `get_root_symbols`, `marked_closure_envs`, and `operative`'s
  custom constructor and destructor. Removing those also fixes the copy/move
  root imbalance described in the postmortem.
* Change `env_root_ptr` to `env_ptr` everywhere. Start with a type alias; the
  compiler will flag the `.get()` calls and the `value::make(env_root_ptr)`
  overload.

## Phase 3: Collection scheduling

* Stop collecting on every eval step (`if (++count > 0)` in `eval`). That is
  what makes the current collector quadratic.
* Instead, check in `environment::make`: collect when the number of
  environments created since the last collection exceeds
  `max(threshold, k × survivors)`.
* Add a stress mode (a debug category or an environment variable) that
  collects on every `environment::make`.

## Phase 4: Documentation

* Record these invariants in `env-gc.md`:
  * C++ code holds environments and values by `shared_ptr` across any call
    that can allocate. A reference into a value is fine while a local
    `value_ptr` keeps that value alive.
  * `builtin_operative` functions must not capture `value_ptr` or `env_ptr`.
    Breaking this rule causes a leak, not a crash.
* Correct the claim in `gc.md` that `set!` can't create cycles.
* Update `TODO.md`. Some items become unnecessary, e.g. "Add child tracking to
  environments so that garbage collection may be done during top-level
  evaluations".

## Phase 5 (optional, needs a decision): Stop creating a cycle on every call

Make `wrap` a primitive, implemented as an "applicative" flag on `operative`
as the README suggests for a Noeval 2, or at least make `eval-list` a builtin.
Either removes the `eval-list-helper` cycle, which accounts for about 41K of
the 42K cycles in the library tests. This changes observable behavior (it
enables `unwrap`, `applicative?`, etc.).

A smaller optimization, if collection is too slow: give each value a
"may contain an environment" flag when it is constructed, and skip scanning
code that can't contain one.

## Done when

* The C++ tests and library tests pass, including under stress mode.
* The Phase 0 GC tests pass, and the postmortem's micro-experiments leak
  nothing extra.
* Running `:reload` repeatedly keeps the registered environment count flat.
* All environments are destroyed at exit.
* The library tests take about as long as the pure reference counting build
  did (around 19 seconds on the machine used for the postmortem), not many
  minutes.

## Build note

The Makefile needs GCC 15 (`<print>`, range formatting in `std::format`).
Building with GCC 14 needs the small patches described in the postmortem's
"Reproducing" section. Either use GCC 15, or make those portability fixes as
part of Phase 0.

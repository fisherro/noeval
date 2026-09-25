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

Status: done. The GC tests are `run_gc_tests()` in `src/tests.cpp`. They run at
the end of `run_tests()`, and on their own with `bin/noeval --gc-tests` (the
existing C++ tests currently abort before reaching them). With the current
collector, all of them fail with `Unbound variable`. With collection disabled
(pure reference counting), a no-cycle control passes and each cycle test
reports exactly the expected leak.

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

Status: done. The collector is `cycle_collector` in `src/noeval.cpp`. The root
machinery is still present but no longer used for collection.

* All C++ tests pass, including the GC tests. Before this phase the C++ tests
  aborted with `Unbound variable`.
* The library tests pass, and the number of live environments is the same
  (705) after each of two consecutive `:reload`s.

`eval` still collects on every evaluation step, and each collection scans all
the library code, so anything that loads the library is very slow: the
library-dependent GC tests didn't finish in 8 minutes. For verification, the
library-dependent GC tests were run while collecting every 97 steps, and the
library tests while collecting every 997 steps. Phase 3 (scheduling) should
probably come before Phase 2.

## Phase 2: Delete the root machinery

* Delete `env_root_ptr`, the `roots` map, `add_root`/`remove_root`,
  `dump_roots`, `get_root_symbols`, `marked_closure_envs`, and `operative`'s
  custom constructor and destructor. Removing those also fixes the copy/move
  root imbalance described in the postmortem.
* Change `env_root_ptr` to `env_ptr` everywhere. Start with a type alias; the
  compiler will flag the `.get()` calls and the `value::make(env_root_ptr)`
  overload.

Status: done (after Phase 3).

* The root machinery is gone, along with the `gc_roots` debug category and
  the `#<environment-root:...>` string form.
* REPL tab completion used `get_root_symbols()`. It now uses the symbols
  visible from the REPL's own environment.
* An empty `NOEVAL_GC_STRESS` (or `0`) now means stress mode is off. Before,
  any value turned it on, and an empty one collected on every allocation.
* All C++ and library tests pass, with the same output as before, also under
  `NOEVAL_GC_STRESS=37`. A full run takes 24 seconds with `-O2` and 123
  seconds with the Makefile's default flags.

## Phase 3: Collection scheduling

* Stop collecting on every eval step (`if (++count > 0)` in `eval`). That is
  what makes the current collector quadratic.
* Instead, check in `environment::make`: collect when the number of
  environments created since the last collection exceeds
  `max(threshold, k × survivors)`.
* Add a stress mode (a debug category or an environment variable) that
  collects on every `environment::make`.

Status: done (before Phase 2, as suggested above).

* `eval` and `top_level_eval` no longer collect. `environment::make` calls
  `maybe_collect()`, which collects once the environments created since the
  last collection reach `max(1000, survivors)`.
* `NOEVAL_GC_STRESS=n` collects every `n` environment creations (`1` means
  every one). The GC tests use stress mode with `n = 1` and 100 iterations.
* Results, building with `-O2`:
  * C++ tests (including the GC tests) and library tests pass. A full run of
    both takes 95 seconds, and 765 environments are live afterwards.
  * They also pass with `NOEVAL_GC_STRESS=37` (472 seconds).
  * Sampling stacks during the library tests puts about 20% of the time in
    the collector and about 33% in `value_to_string`: `NOEVAL_DEBUG`
    evaluates its arguments even when its category is off, and
    `call_stack::guard` converts every expression to a string. That, not the
    collector, is now the bigger cost.
  * Follow-up: `NOEVAL_DEBUG` now only evaluates its arguments when its
    category is enabled, and the call stack stores expressions and only
    converts them to strings when formatting a stack trace. The full run
    went from 107 to 29 seconds with `-O2` (164 seconds with the Makefile's
    default flags), with identical output. The collector is now about half
    of the remaining time.
* The Makefile doesn't enable optimization, so the default build is several
  times slower than the numbers above.

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

Status: done.

* `env-gc.md` now describes the current collector, when it runs, the rules for
  C++ code, and how to test and debug it.
* `gc.md` has a pointer to `env-gc.md`, and its claims about `set!` and
  `env_root_ptr` are corrected.
* `TODO.md` no longer lists the GC items that are done (collection points,
  child tracking, `environment::unregister`).
* `noeval-reference.md` describes the current collector.

## Phase 5 (optional, needs a decision): Stop creating a cycle on every call

Make `wrap` a primitive, implemented as an "applicative" flag on `operative`
as the README suggests for a Noeval 2, or at least make `eval-list` a builtin.
Either removes the `eval-list-helper` cycle, which accounts for about 41K of
the 42K cycles in the library tests. This changes observable behavior (it
enables `unwrap`, `applicative?`, etc.).

A smaller optimization, if collection is too slow: give each value a
"may contain an environment" flag when it is constructed, and skip scanning
code that can't contain one.

Status: done, by making `eval-list` a builtin. `wrap` stays in the library:
making it a primitive changes the language, and the README leaves that for a
Noeval 2. The "may contain an environment" flag wasn't needed.

* `eval-list` is now `eval_list_operative` in `src/noeval.cpp`, and the
  library definition is gone. It evaluates its arguments in the same order
  as before (the environment, then the list) and raises the same error for a
  non-list.
* A new GC test, "lambda call without cycles", calls a `lambda` with
  collection held off and checks that reference counting alone frees every
  environment. It fails with the old library `eval-list` (20 environments
  left after 10 calls).
* Library tests (`:reload`, after startup): environments collected went from
  461,891 (in 3,269 collections) to 47,378 (in 827 collections).
* With `-O2`, a full run (C++ tests plus `:reload`) went from 22 to 5.6
  seconds. It takes 102 seconds with `NOEVAL_GC_STRESS=37` (the baseline
  wasn't measured under stress mode on this machine). The output is the
  same, except that fewer environments are live afterwards
  (744 instead of 765 after `:reload`, 246 instead of 299 after
  `:reload fast`), and that count stays flat across repeated reloads.
* With the Makefile's default flags, a full run now takes 31 seconds (the
  Phase 3 numbers were measured on a different machine, so they aren't
  directly comparable).

## Done when

* The C++ tests and library tests pass, including under stress mode.
* The Phase 0 GC tests pass, and the postmortem's micro-experiments leak
  nothing extra.
* Running `:reload` repeatedly keeps the registered environment count flat.
* All environments are destroyed at exit.
* The library tests take about as long as the pure reference counting build
  did (around 19 seconds on the machine used for the postmortem), not many
  minutes. (The test suite has grown since then, and much of the current
  cost is debug string building rather than collection. See Phase 3.)

## Build note

The Makefile needs GCC 15 (`<print>`, range formatting in `std::format`).
Phase 0 made the portability fixes, so it also builds with GCC 14
(`make CXX=g++-14`).

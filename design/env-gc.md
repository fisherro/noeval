# Environment garbage collection

## Background

We use `std::shared_ptr` for Noeval values (`value_ptr`) and environments
(`env_ptr`). Reference counting frees most garbage immediately. What it can't
free is cycles, and those are common. For example, defining a local operative
creates a cycle: environment → binding → operative → closure environment (the
same environment). Other cycles come from `define-mutable` + `set!`, an
environment bound in itself, and environment parameters. See
[gc-postmortem.md](gc-postmortem.md) for measurements.

Every cycle passes through an environment:

* Cons cells are immutable and built from existing values, so they can't form
  a cycle on their own.
* `eval_symbol` unwraps mutable bindings, so Noeval code never gets hold of a
  `mutable_binding` itself.

## The collector

`environment::collect()` runs a trial-deletion cycle collector
(`cycle_collector` in `src/noeval.cpp`). It is the same idea as Bacon–Rajan or
CPython's `gc` module:

1. **Count.** Every environment adds itself to a registry when constructed and
   removes itself when destroyed. Starting from the registered environments,
   find every environment and value reachable from them. Give each one a count
   equal to its reference count (`weak_from_this().use_count()`), then
   subtract one for each reference to it found inside the scanned heap.
2. **Mark.** Anything whose count is still positive is referenced from outside
   the heap (the C++ stack, a `tail_call`, the REPL's environment, ...). Those
   are the roots. Mark everything reachable from them.
3. **Break.** Unmarked environments are garbage. Hold strong references to all
   of them, clear their bindings and parents, then release them.

No root registration is needed. A `shared_ptr` held by C++ code shows up as an
extra reference count, which makes the object a root automatically.

## When collection happens

* `environment::make` calls `maybe_collect()`. It collects once the number of
  environments created since the last collection reaches the number that
  survived the last collection (but at least 1,000). So collection happens in
  the middle of evaluations, not just between them.
* `reload_top_level_environment`, the end of `main`, and the GC tests call
  `environment::collect()` directly.

## Avoiding cycles in hot code

Collection time grows with the amount of cyclic garbage, so code that runs
on every call shouldn't create cycles. A local operative defined inside an
operative's body (a helper function, for example) creates one on every call.
`eval-list` used to be a library operative with a local helper, and `wrap`
calls it every time a wrapped operative is called. That accounted for about
90% of the environments collected while running the library tests, so it is
now a builtin. The GC test "lambda call without cycles" checks that calling
a `lambda` leaves nothing for the collector.

## Rules for C++ code

1. **Hold environments and values by `shared_ptr`** (`env_ptr`, `value_ptr`)
   across any call that can create an environment, which includes anything
   that evaluates Noeval code. A raw pointer or reference into a value is fine
   only while some `shared_ptr` in the C++ code keeps that value alive.
   Otherwise, something still in use can be collected (its bindings cleared).
2. **Count each reference exactly once.** `cycle_collector::for_each_reference`
   must report every `value_ptr` and `env_ptr` held by an environment or value
   once, and nothing else. If you add a new kind of value, or a new member that
   holds a `value_ptr` or `env_ptr`, update it.
   * Reporting a reference twice (or one that doesn't exist) can make live
     objects look like garbage.
   * Missing a reference only causes a leak: the object it refers to looks
     like it's referenced from outside the heap, so it's kept.
3. **`builtin_operative` functions must not capture `value_ptr` or `env_ptr`.**
   The collector can't see into a `std::function`, so anything captured is
   kept forever.
4. **Only create environments with `environment::make` and values with
   `value::make`** (both constructors are private), so they are always owned
   by a `shared_ptr`.
5. **Destructors must not evaluate Noeval code.** Breaking cycles destroys
   values and environments in the middle of a collection.

## Testing and debugging

* `bin/noeval --gc-tests` runs only the GC tests (`run_gc_tests()` in
  `src/tests.cpp`). They also run as part of the startup tests.
* `NOEVAL_GC_STRESS=n` collects every `n` environment creations (`1` means
  every one). Unset, empty, or `0` turns stress mode off. The GC tests use an
  interval of 1 internally.
* In the REPL, `:debug on gc` logs each collection, and `:debug env-counts`
  shows how many environments are alive.

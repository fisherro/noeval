# Environment GC postmortem

Question: first-class environments (needed for first-class operatives) created
cycles that broke reference counting, and attempts to patch around that failed.
If Noeval had used conventional special forms instead of first-class operatives,
would cycles still have been a problem?

Short answer: Yes. Almost all of the cycles are ordinary closure cycles.
First-class environments made the leaks about 2.6× larger, and fexprs ruled out
the usual techniques for avoiding cycles by construction, but they are not what
created the cycles.

## Measurements

### Classifying the leaks in the pure reference counting version

Commit `9a609ff` (pure `shared_ptr`, no collector) says the library tests leak
109,558 environments. Rebuilding that commit reproduces exactly that number.

I instrumented that build to keep a set of all live environments. After
teardown, everything still alive is garbage kept alive by a cycle. I then built
a graph of the leaked environments with three kinds of edges:

* parent: `environment::parent`
* closure: a binding's value (possibly nested in cons cells or mutable
  bindings) is an operative whose `closure_env` is the target
* env-value: a binding's value is an `env_ptr` (i.e. a first-class environment
  from a `vau` environment parameter or `get-current-environment`)

For each edge set, I computed the environments that lie on a cycle (Tarjan's
SCC) and the environments kept alive (reachable from a cycle).

| Edges considered                 | Environments on a cycle | Environments leaked |
|----------------------------------|------------------------:|--------------------:|
| All                              | 42,277                  | 109,558             |
| Without env-value edges          | 42,044                  | 42,048              |
| Without closure edges            | 5                       | 5                   |

* Almost every cycle goes through a closure. Only 233 of 42,277 cycle members
  depend on a first-class environment edge.
* First-class environments amplify the leak. Environment parameter bindings
  (`wrapped-call-env`, `current-env`, `env`, ...) point from a leaked cycle to
  the caller's environment, so each leaked cycle drags its callers along with
  it.
* One pattern dominates: 41,713 cycles come from `eval-list-helper`, which
  `eval-list` defines locally. Every applicative call goes through
  `wrap` → `eval-list`, so every applicative call leaks a cycle.

### Micro-experiments

Using the same build, each operative below was called 1,000 times from the REPL.
None of them use `wrap` or an environment parameter except the last.

| Test                                                        | Extra leaked |
|-------------------------------------------------------------|-------------:|
| `(vau () () (do (define helper (vau () () 1)) 0))`          | 1000         |
| `(vau () () (do (define x 1) 0))`                           | 0            |
| `(vau () () (vau () () 1))` (returns a closure)             | 0            |
| `(vau () env (do (define saved env) 0))`                    | 0            |

A local operative that is never called and doesn't refer to itself still
leaks one environment per call. Saving the caller's environment does not leak
on its own.

## Why the cycles exist without fexprs

Noeval closures capture their entire environment. An internal `define` stores
the closure in that same environment. So *every* local function definition is a
cycle: environment → binding → operative → `closure_env` → environment. It
doesn't need to be recursive or even called.

Every `(define loop ...)` in `lib.noeval` therefore leaks one cycle per call of
its enclosing function. A language with conventional special forms but the same
closure representation and internal `define` would behave the same way.

`define-mutable` followed by `set!` to a closure can also create a cycle,
contrary to what `gc.md` says.

## What fexprs specifically cost

1. `lambda` is library code built on `wrap` and `eval-list`, so every function
   call creates and leaks a cycle. With `lambda` as a primitive special form,
   that dominant cycle disappears.
2. Amplification: roughly 42K leaked environments become roughly 110K because
   environment parameters tie cyclic garbage to caller environments.
3. They block the standard ways to avoid cycles. Flat closures (capture only
   the free variables) and lambda lifting (turn local recursive functions into
   top-level ones) both require knowing statically which variables a body uses.
   With `eval` in arbitrary environments and reified environments, that can't
   be known, so whole-environment capture is forced.

As far as I know, Lean 4 and Koka (Perceus) use plain reference counting with no
cycle collector for exactly this reason: immutable data plus lifted local
recursive functions means cycles can only arise through mutable references,
which they accept may leak.

## Why the patches failed

This is a separate problem from the cycles themselves.

Building the final shelved version (`b6d8fc4`), the C++ tests abort with
`Unbound variable: eval`. Two root-counting bugs contribute:

* `operative` has a user-declared destructor, so no move constructor is
  generated, and `value::make(operative{...})` copies it. The implicit copy
  constructor doesn't call `add_root`, but both the temporary's and the copy's
  destructors call `remove_root`. Each `vau`-created operative therefore
  removes one root count that belongs to someone else. Live environments lose
  their root status and `sweep` clears their bindings.
* The `marked_closure_envs` workaround in `cleanup_registry` removes one more
  root count from every reachable closure environment on every collection, and
  collection runs on every eval step (`if (++count > 0)`).

With the `marked_closure_envs` workaround disabled, the library tests fail with
exactly the `Unbound variable: do` / `cond-transformer` error recorded in
`transcript-notes.txt`.

With an explicit copy constructor that calls `add_root` as well, the corruption
went away for the portion of the library tests I ran, but it leaked steadily:
393 registered environments after 20,000 collections, 2,305 after 160,000, with
the root count growing alongside.

That exposes the core contradiction: making each operative root its closure
environment protects operatives held only by C++ temporaries, but it equally
protects operatives inside garbage cycles, so those cycles are never collected.

This root-finding problem is not specific to fexprs. Any tracing collector added
on top of `shared_ptr` and the C++ stack has to find roots somehow.

## Options for a future language

* Conventional special forms, avoiding cycles by construction: flat closures,
  lambda-lift local recursive functions (or compile internal `define` as
  `letrec` and lift it), and put mutation in explicit boxes. Plain reference
  counting then works except for cycles through those boxes.
* Keeping fexprs or whole-environment closures: use a trial-deletion cycle
  collector (Bacon–Rajan, as in CPython's `gc` module). It compares each
  object's reference count (`use_count()`) against references found inside the
  heap. Anything with extra references, such as a `shared_ptr` held in a C++
  local, is automatically treated as a root. No `env_root_ptr` discipline is
  needed, which addresses Noeval's actual failure.

So fexprs don't need to be ruled out on garbage-collection grounds. What they
cost is the option to avoid cycles by design, which means planning for a real
cycle collector from the start.

## Reproducing

The measurements used scratch builds, not changes to this repository.

* GCC 14 (Ubuntu 24.04) needed small local patches: formatting ranges with
  `std::format` isn't supported until GCC 15, so `join_with` results were
  converted with `std::ranges::to<std::string>()`, and `<set>`/`<map>` had to be
  included explicitly in `noeval.hpp`.
* For the leak classification, `environment` gained a static
  `std::unordered_set<environment*>` of live instances maintained by its
  constructor and destructor, and `main` analyzed it after `global_env.reset()`.
* The REPL loops forever on EOF, so input must end with `quit`.

# Contributing

Noeval is a fexpr-based Lisp inspired by John Shutt's Kernel. The
[README](README.md) explains the goals of the project, how to build and run
it, and where Noeval differs from Kernel. [STYLE.md](STYLE.md) has the style
rules for C++ and Noeval code.

## Philosophy

- **Minimize primitives.** Implement as much as possible in the language
  itself. Add a primitive only when a library implementation is impractical.
- **Operatives control evaluation.** An operative (fexpr) receives its
  operands unevaluated, along with the environment it was called from, and
  decides what to evaluate and when. When writing one, be deliberate about
  what gets evaluated, and in which environment.
- **Church Booleans instead of a primitive conditional.** A condition
  evaluates to `true` or `false`, which select between alternatives:
  `(condition true-branch false-branch)`.
- **Sequencing is separate from scoping.** `do` and `try` don't create
  environments; `let` and `lambda*` do. So a multi-expression body doesn't
  need an environment of its own, and a helper that shouldn't be visible
  outside a definition belongs inside a `lambda*` or `let`.
- **Immutable by default.** Mutation is explicit: only bindings created with
  `define-mutable` can be changed with `set!`.

## Building and testing

GCC 14 or later is required. If your default `g++` is older, pass it to make,
e.g. `make CXX=g++-14`. Run noeval from the top of the repository, since it
loads the library and tests by relative path.

- `make` builds the optimized interpreter, `bin/noeval`.
- `make test` runs the C++ tests, the library tests, and the garbage
  collection tests under stress.
- `make test-sanitize` runs the same tests with AddressSanitizer and
  UndefinedBehaviorSanitizer, and with the library tests under GC stress.
- `make bench` times the benchmarks. To measure a change, compare against
  results saved from before it with `benchmarks/run.bash -c`.

`./check-reference.bash` lists the builtins and library definitions that
[noeval-reference.md](noeval-reference.md) doesn't mention. Run it after
adding one.

CI runs `make test`, `make test-sanitize`, and each benchmark once.

## Conventions

### Built-in operatives

Built-ins follow a consistent pattern: validate the arguments, evaluate
selectively, and return a value.

### Errors

- Re-throw `evaluation_error` instances unchanged.
- Wrap other exceptions with context.
- Use `expr_context()` for readable error messages.
- Give specific error messages, not generic ones.

### Debugging

The interpreter has category-based debug logging (see `src/debug.hpp`). In
the REPL, `:debug` turns categories on and off.

## Tests

When implementing a feature:

- Add C++ tests (`src/tests.cpp`) for low-level functionality.
- Add library tests (`tests/*.noeval`) for user-visible behavior.
- Test error conditions, including their messages.
- Check that operatives work with unevaluated operands.

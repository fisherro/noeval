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
  `define-mutable` can be changed with `set!`, and `define` can't rebind a
  name in the same environment.

## Building and testing

GCC 14 or later is required. If your default `g++` is older, pass it to make,
e.g. `make CXX=g++-14`. Run noeval from the top of the repository, since it
loads the library and tests by relative path.

- `make` builds the optimized interpreter, `bin/noeval`.
- `make test` runs the C++ tests, the library tests, and the garbage
  collection tests under stress.
- `make test-sanitize` runs the same tests with AddressSanitizer and
  UndefinedBehaviorSanitizer, and with the library tests under GC stress.
- `make bench` times the benchmarks and reports their peak memory use. To
  measure a change, build the earlier commit in a `git worktree` and compare
  against it with `-a`, which runs the two checkouts' binaries alternately,
  benchmark by benchmark:

  ```bash
  git worktree add ../noeval-before HEAD~1
  make -C ../noeval-before
  benchmarks/run.bash -a ../noeval-before
  ```

  The ratios are this checkout's time over the earlier one's. A machine's
  speed can drift a lot between runs, even within a few seconds, so compare
  runs made alternately like this rather than results saved earlier
  (`benchmarks/run.bash -c`). Both checkouts run this checkout's copy of each
  benchmark, but each loads its own library, which `-b` alone wouldn't do.
  Timing can't settle a difference of a few percent, since runs can be faster
  as well as slower than usual. For that, count the instructions executed in
  each checkout, which doesn't depend on the machine's speed:

  ```bash
  valgrind --tool=cachegrind --cache-sim=no --cachegrind-out-file=/dev/null \
      bin/noeval --skip-tests benchmarks/fib.noeval
  ```

`./check-reference.bash` lists the builtins and library definitions that
[noeval-reference.md](noeval-reference.md) doesn't mention. Run it after
adding one.

CI runs `make test`, `make test-sanitize`, and each benchmark once.

Don't add Python to the repository, for tooling or anything else. Scripts are
Bash. If a tool needs information only the interpreter has, noeval can provide
it, as it does for the benchmarks' peak memory (`NOEVAL_REPORT_PEAK_MEMORY`).

## Conventions

### Built-in operatives

Built-ins follow a consistent pattern: validate the arguments, evaluate
selectively, and return a value. Check the number of arguments with
`expect_args`, which gives the standard error message.

### Macros

An operative that only builds code and evaluates it in the calling
environment should be a macro: `(macro (vau operands _ expansion))`. The
expansion is built once for each combination and cached (see
[design/macros.md](design/macros.md)), so checks on the operands are made once
too.

- **A transformer depends only on its operands.** It gets an empty
  environment, and it runs once per combination, not once per call, so it
  shouldn't have side effects.
- **Embed values, not names.** Build the expansion with `list` and `cons`, so
  that it contains the values of `if`, `do`, and so on, which a local binding
  in the calling environment can't change.
- **Don't introduce bindings in an expansion.** They could capture the names
  in the user's code. Put temporaries inside an embedded operative instead,
  which evaluates the user's code in the calling environment.
- **Capture only on purpose.** When capturing a name is the point, as in an
  anaphoric macro that binds `it` for its body, insert that one name as a
  symbol, embed values for everything else, and document the name.
- **Don't add a scope.** The expansion is evaluated in the calling
  environment, so a `define` in it binds there, as it would in `do`. Wrapping
  the expansion in a `lambda` would change that.

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

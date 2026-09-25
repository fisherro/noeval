# Agent instructions

Read [CONTRIBUTING.md](CONTRIBUTING.md) for the project's philosophy,
conventions, and how to build and test, and [STYLE.md](STYLE.md) for the
style rules. [noeval-reference.md](noeval-reference.md) summarizes the
language for working on Noeval code without reading the interpreter.

## Working approach

- **Don't make assumptions or jump to conclusions.** Take time to analyze
  fully and come up with the correct answer, or ask clarifying questions or
  suggest tests to determine missing information.
- **Verify before asking.** Don't ask a question or suggest a test until
  you've confirmed you don't already have access to the needed information.
- **Aim for accuracy and completeness.** Take time to give thorough, correct
  answers on the first attempt.

## Commands

Run these from the top of the repository. If the default `g++` is older than
14, add `CXX=g++-14`.

- `make test`: build and run all the tests
- `make test-sanitize`: run the tests under the sanitizers
- `make bench`: time the benchmarks
- `bin/noeval --skip-tests script.noeval`: run a script without running the
  C++ tests first

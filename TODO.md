# TODO

<!-- markdownlint-disable MD025 -->

## Ongoing

What primitives can we get rid of?

Should any library functions take advantage of other library functions or
primitives that they aren't?

Review code for conformance to the style guidelines

Look for redundant or unneeded library tests that we can remove to keep the
test suite size down.

Update noeval-reference.md

## Planned

Concrete, scoped work that could be picked up without first deciding whether
or how to do it.

`read` still pulls characters from `std::cin` one at a time. The lexer's
`pushback_streambuf` reads its source in chunks only when `in_avail()` says
characters are ready, and `std::cin`'s buffer, while synced with stdio, always
reports none, so every character costs a couple of virtual calls and a
`getc()`. (Loading files is unaffected: `istringstream` reports its whole
contents as ready, and parsing a 7.4 MB file is back to the speed it had
before the lexer read from streams.) So far `read`'s time is dominated by
evaluation, so this hasn't mattered. If it does, give `pushback_streambuf` a
source that reads the file descriptor directly (`read(2)` returns what's
available without waiting for more). When stdin isn't a terminal, the REPL
reads through `std::cin`'s stream buffer too (via `rl_getc_function`), so it
would share that source.

Add max-garbage stat

## Ideas

Questions, things to consider, and open-ended design work.

`cond`'s helpers (`cond-clauses`, `cond-clause`, `cond-test`, and
`cond-body`) are global names. Should they be hidden? Defining them inside
`cond` would rebuild them on every call, which would give back some of the
speed `cond` gained by evaluating its clauses directly.

Implement transducers (See Clojure and SRFI-171)

Prioritize macros and RRB trees in order to improve performance.

Member functions to extract values from the value type.

A special form for "requires contracts" to standardize argument checking?

File I/O

map*

Other HOF

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

First-class delimited continuations

Add expansion-time macros (see [having-both-fexprs-and-macros.html](https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html) )

* A `macro` primitive works kind of like `wrap` to turn any operative into a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body before storing it in the operative.

Argument-count checks in the builtin operatives share `expect_args`, but
there's more common code that could be refactored, such as the blocks that
rethrow evaluation errors and wrap other exceptions.

Pattern matching?

Floats (boost::multiprecision::cpp_dec_float)? Symbolic irrationals? Wait until modules? Rationals are never implicitly converted to floats? Use IEEE floats first as anyone doing floating point work already knows the caveats and will expect them to apply?

Use demangle in value_to_string?

Revisit the uses of std::visit

Look for places we could simplify code with ranges

Revisit escaping in string literals (what scheme do we want to use?)

Replace lists with arrays? Or maybe Clojure/Scala style vectors? Or RRB trees?
And maybe going further, support for homogenous RRB trees of specific types (like bytes) would make sense?

> I'll avoid set-car!/set-cdr! for now...a set-array-element! might happen
> And that might be needed for a good promise implementation

Consider adding an equivalent to Kernel's #ignore

Module system?

Hash sets and maps (Should this be done my making environments first class?)

Interning symbols

Support for lazy evaluation (We can do this in the library, right?)

FFI and POSIX support

Look for places we can use string_view

Consider whether to adopt Kernel's $ naming convention

Implement void or #inert?

Dynamic variables (a la Kernel?) (could be used for test-failures in the library tests)

Consolidate write and display into a single primitive

Consider if*, cond*, and*, or*, not* library functions that can handle
non-boolean values

Rewrite any uses of if into Church Boolean form in the library

Do we need get-current-environment? Is it correct?

`for-each*` that can handle *n*-ary procedures?

Makes `value` formattable by std::format and std::print and then use them to
expose formattting functions to Noeval.

Function overloading or multimethods? Some way to allow the list functions to work on any sequence.

User-defined types?

Consider switching to intrusive reference counting

Once there are expansion-time macros, try the old `cond` again. It's kept in
a `#skip` block in `src/lib.noeval`. It transformed its clauses into an `if`
chain and evaluated that, but it redid the transformation every time, which
made it about 14 times slower than the `if` chain alone. The current `cond`
evaluates its clauses directly, which is about 1.7 times slower than the `if`
chain. A macro could transform once and also check every clause up front.
(Benchmarks: `cond`, `if-chain`, and `codepoints-utf8`.)

`string-length`, `string-nth`, and `substring` convert the whole string to a
list on every call, so indexing a string in a loop is quadratic.
(Benchmarks: `string-index` and `substring`.) For now, we'll say that the
best practice is to convert to a list, do the manipulations there, and then
convert back. But we may want to reconsider at some point.

Think about how we could "pre-compile" the library to improve the time it
takes to load it on startup. (And whether that's even worth doing.)

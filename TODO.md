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

`read` and the REPL can steal each other's input when stdin isn't a terminal.
`read` parses `std::cin`, while the REPL reads through readline. When stdin is
a pipe or file, stdio fills its buffer with a whole block, so after
`(read)`, lines meant for the REPL may be sitting in `std::cin`'s buffer
where readline never sees them. (On a terminal, input arrives a line at a
time, so it works.) Fixing this would mean having the REPL and `read` share
one input buffer, or disallowing `read` from the REPL. The lexer's
`pushback_streambuf` could be that buffer: it wraps `std::cin`'s stream buffer
(which, while synced with stdio, reads the C `stdin` `FILE*`), and readline
can be pointed at it with `rl_getc_function`, so both would consume the same
characters in order.

`read` still pulls characters from `std::cin` one at a time. The lexer's
`pushback_streambuf` reads its source in chunks only when `in_avail()` says
characters are ready, and `std::cin`'s buffer, while synced with stdio, always
reports none, so every character costs a couple of virtual calls and a
`getc()`. (Loading files is unaffected: `istringstream` reports its whole
contents as ready, and parsing a 7.4 MB file is back to the speed it had
before the lexer read from streams.) So far `read`'s time is dominated by
evaluation, so this hasn't mattered. If it does, give `pushback_streambuf` a
source that reads the file descriptor directly (`read(2)` returns what's
available without waiting for more), which could also be shared with the REPL
as described above.

Have the parser track the file path so that `load` can use its directory as the "current directory" for relative paths.

Add max-garbage stat

Ensure REPL tab completion works for REPL special commands

Add validation of the bindings structures to let

## Ideas

Questions, things to consider, and open-ended design work.

Implement transducers (See Clojure and SRFI-171)

Should use of `read` be prevented from the REPL?

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

There's a lot of common code that could be refactored in the builtin operatives

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

Consider `do` and `try` creating their own environment

Function overloading or multimethods? Some way to allow the list functions to work on any sequence.

User-defined types?

Consider switching to intrusive reference counting

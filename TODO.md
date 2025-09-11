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

## Regular

Remove global environment.

Collect at top of eval loop (maybe not every time).

Should use of `read` be prevented from the REPL?

Change parser to use a "stream" "adaptor" that can wrap stdin or std::cin and
provide arbitrary pushback.

Fix the read builtin so that it doesn't read everything up-front

Have the parser track the file path so that `load` can use its directory as the "current directory" for relative paths.

Capture the accumulator and the accumulate-reverse patterns in library forms.
Or, more specifically, provide `unfoldl`, `unfoldr`, `foldl-until`, and
`foldr-until`. Use these to implement all/most of the other list processing
library functions.

Prioritize macros and RRB trees in order to improve performance.

Member functions to extract values from the value type.

Add child tracking to environments so that garbarge collection may be done
during top-level evaluations.

A special form for "requires contracts" to standardize argument checking?

File I/O

environment::unregister:
Does nothing when collection is in progress.
Otherwise immediately removes.
Then dtor will call unregister.
That way the dtor removing from the registry can't mess up collection.
And add max-constructed stat to measure this improvement.

Add max-garbage stat

Ensure REPL tab completion works for REPL special commands

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

Add validation of the bindings structures to let

Add GC collection points to more places.

Function overloading or multimethods? Some way to allow the list functions to work on any sequence.

User-defined types?

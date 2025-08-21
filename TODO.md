# TODO

<!-- markdownlint-disable MD025 -->

## Ongoing

What primitives can we get rid of?

Should any library functions take advantage of other library functions or
primitives that they aren't?

Review code for conformance to the style guidelines

Look for redundant or unneeded library tests that we can remove to keep the
test suite size down.

## Regular

update noeval-reference

string library functions: string-ref, substring

more string library functions: strings->string, string->codepoint-string

UTF-8 library functions: codepoints->utf8, utf8->codepoints

Make value ctor private and provide a value::make static member function

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

Support for loading and executing a file listed on the command line

Maybe an "include file" that would allow breaking the tests up into multiple files? Or would it be better to implement modules?

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

First-class delimited continuations

Add expansion-time macros (see [having-both-fexprs-and-macros.html](https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html) )

* A `macro` primitive works kind of like `wrap` to turn any operative into
* a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body before storing it in the operative.

There's a lot of common code that could be refactored in the builtin operatives

Pattern matching?

Floats (boost::multiprecision::cpp_dec_float)? Symbolic irrationals?

Use demangle in value_to_string?

Revisit the uses of std::visit

Look for places we could simplify code with ranges

Revisit escaping in string literals (what scheme do we want to use?)

Replace lists with arrays? Or maybe Clojure/Scala style vectors? Or RRB trees?

> I'll avoid set-car!/set-cdr! for now...a set-array-element! might happen
> And that might be needed for a good promise implementation

Consider adding an equivalent to Kernel's #ignore

Module system?

Hash sets and maps

Interning symbols

Support for lazy evaluation (We can do this in the library, right?)

FFI and POSIX support

Look for places we can use string_view

Consider whether to adopt Kernel's $ naming convention

Would it make sense to implement `if` in terms of `cond`?

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

Move the testing infrastructure into the library?
Or wait until we have modules and put it in a module?

Consider `do` and `try` creating their own environment

Add validation of the bindings structures to let

Add GC collection points to more places.

Function overloading or multimethods

User-defined types?

# TODO

## Ongoing

What primitives can we get rid of?

Should any library functions take advantage of other library functions or
primitives that they aren't?

Review code for conformance to the style guidelines

## Regular

map and other HOF

append, length, reverse

filter

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

TCO

First-class delimited continuations

Add expansion-time macros (see https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html )

* A `macro` primitive works kind of like `wrap` to turn any operative into
* a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body before storing it in the operative.

There's a lot of common code that could be refactored in the builtin operatives

Pattern matching?

"Numeric tower"...or at least support of bignums

* All numbers are boost::cpp_rational
* Default formatting is (possibly) repeating decimals
* Floats will be boost::cpp_dec_float

Use demangle in value_to_string?

Revisit the uses of std::visit

Look for places we could simplify code with ranges

Revisit escaping in string literals (what scheme do we want to use?)

Replace lists with arrays?

> I'll avoid set-car!/set-cdr! for now...a set-array-element! might happen
> And that might be needed for a good promise implementation

Consider adding an equivalent to Kernel's #ignore

Module system?

Hash sets and maps

Interning symbols

Support for lazy evaluation (We can do this in the library, right?)

FFI and POSIX support

Create let

* Advantages of let over define...
* Bindings only happen at the top of the let and cannot be rebound within the body
  * We'd need to ensure that define can't rebind something bound with let
* More functional...define does a mutation

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

Change equal_operative to use op== and/or use other type-specific customization point

Move the testing infrastructure into the library?
Or wait until we have modules and put it in a module?

# TODO

What primitives can we get rid of? (ongoing)

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

Add expansion-time macros (see https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html )

* A `macro` primitive works kind of like `wrap` to turn any operative into
* a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body
* before storing it in the operative.

Pattern matching?

Need to be able to catch errors in tests. Ideally that happens after we add
continuations, but it may be important enough to do something temporary.

"Numeric tower"...or at least support of bignums

* All numbers are boost::cpp_rational
* Default formatting is (possibly) repeating decimals
* Floats will be boost::cpp_dec_float

TCO

Delimited continuations

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

Simplify `if`

> `(if a b c)` is the same as `(a b c)`

`for-each*` that can handle *n*-ary procedures?

Change equal_operative to use op== and/or use other type-specific customization point

Move the testing infrastructure into the library?
Or wait until we have modules and put it in a module?

## Style:

Ensure catches and elses are cuddled

Ensure functions have open brace on new line (but only for functions)

Ensure question marks and colons have no space before and a space after

Ctor initializer lists have no space before the colon

Ensure space beween keywords and open parenthesis (but not for function calls)

Use and, or, and not keywords instead of &&, ||, and !

Prefer abbreviated templates, especially when naming the type is unnecessary
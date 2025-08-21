# Strings in Noeval

Noeval has:

* string as a builtin type (`typeof` on a string returns the `string` symbol)
* support for string literals
* support for writing and displaying strings

Strings are intended for text rather than binary data. While conversion between
strings and bytes will be possible, it is not a core functionality for strings.

## Primitive additions

In keeping with the minimal primitives design, Noeval will provide these
string-related primitives.

* `string->list`: Returns a list of numbers representing the Unicode codepoints of the string.
* `list->string`: Returns a string from a list of numbers representing Unicode codepoints. If any element of the list is not a number or not a valid unicode codepoint, an error will be raised.

## Possible library additions

Potential string-related library functions include

* `string-length` which gives the number of codepoints
* `string-ref` which will return a single codepoint string
* `substring`
* `string-append`
* `codepoints->utf8` Takes a list of numbers representing Unicode codepoints and returns a list of numbers representing the corresponding UTF-8 byte values. Invalid Unicode codepoints will raise an error.
* `utf8->codepoints` Takes a list of numbers representing the bytes of a UTF-8 sequence and returns a list of numbers representing the Unicode codepoints of the sequence. Invalid byte values or invalid UTF-8 sequences will raise an error.
* `strings->string` Takes a list of strings and converts them into a single string.
* `string->codepoint-strings` Converts a string into a list of single-codepoint strings.

## Notes

I've considered replacing lists with arrays or a more array-like data
structure. I've considered eliminating strings in favor of arrays of numbers.
We're currently using boost::multiprecision::cpp_rational as our number type,
which (for zero) is 64-bytes on the current architecture. For 32-bit integers,
cpp_rational should not need to allocate memory. Still, they're twice the size
of char32_t, and even for Chinese texts, UTF-8 tends to be about 25% smaller
than UTF-32. So, best case, we're probably still looking at cpp_rational
strings being 60% larger than UTF-8. For straight ASCII, it is > 80%.

# Noeval numbers

The default numbers in Noeval are arbitrary precision rationals.

I had considered including infinity, but have decided against it.
While it would be elegant...

* Introduces special cases in the implementation
* Can obscure where an error crept in
* Have to deal with (- inf inf) and (/ inf inf)

## Serialization

Fractional Noeval rationals can be represented in string form by either a
numerator and a denominator or by a—possibly repeating—decimal. The decimal
form will be the default.

Note that fractional decimal form requires at least a single digit before the
decimal point.

    0
    3
    -3
    0.3
    -0.3
    0.(3)
    -0.(3)
    1/3
    -1/3

In regex form:

    -?[0-9]+(\.[0-9]+)?(\([0-9]+\))?
    -?[0-9]+(/[1-9][0-9]*)?

### Other bases

Here are regex describing the format of numeric literals in other bases:

* Hex: `#[xX][0-9A-Fa-f]+`
* Octal: `#[oO][0-7]+`
* Binary: `#[bB][01]+`
* Arbitrary (from 2 to 36): `#[1-9][0-9]*r[0-9a-zA-Z]+`

Non-decimal literals do not need to suggest negative or fractional values.

For arbitrary bases, the base is always given in decimal.

## Rationale (pun intended)

It's arbitrary...more or less.

I've always been annoyed by high-level languages where numbers have surprising
limitations. For example, I see no reason to not automatically promote to a
higher precision instead of overflowing or underflowing.

Irrational numbers and infinities, however, provide enough complications to,
for me, justify drawing a line there.

Noeval doesn't (yet) need performance, so jumping straight to arbitrary
precision makes for a simpler implementation. Also, it feels like a lot of
cases where a faster integer type matters get handled internally within the
interpreter rather than in Noeval code itself.

If we did implement limited precision numbers for performance, however, I would
want that to be (mostly) hidden from the Noeval code. In that case, it should
simply be that smaller magnitude numbers transparently perform better.

It also always seemed weird to me that most systems print fractional rationals
as slashed-fractions by default. I'm used to decimal fractions being the
default and slashed-fractions being the alternative.

## Operations that could produce irrational numbers

...which would mean having to address irrationals.

* Roots and fractional exponents
* Logarithms
* Trig functions (since they're based on square roots)
* Transcendental constants (e.g. pi & e)

## Inexact numbers

At some point we'll probably want to introduce floats. I lean towards them
being arbitrary precision decimal floats. Arbitrary precision floats, however,
are a more complicated thing than arbitrary precision rationals. Also, that
might not be the best choice for most of the reasons someone might want to use
floats. So, decisions there to be delayed.

## Algebraic numbers

I'm also drawn to the idea of going the symbolic route to support irrational
numbers. That seems like a complication beyond the scope of this project,
however.

# Noeval

Noeval is a [fexpr](https://en.wikipedia.org/wiki/Fexpr)-based[^fexpr] Lispy programming language and its interpreter.

[^fexpr]: "fexpr-based"? Or rather: Based on Shutt's lexically-scoped first-class expansion on the idea of fexprs.

I created Noeval to explore and better understand some programming language concepts.

Noeval is pronounced /ˈnɔɪvæl/. Like part of the phrase "an*noy val*edictorian".

Noeval is inspired by [Kernel](https://web.cs.wpi.edu/~jshutt/kernel.html).

## The plan

Like so many before me, I want to try to keep the interpreter relatively
simple, the primitives powerful, and avoid adding primitives that could be
implemented in the language itself.

Although I will bend to cases where a library implementation is impractical.
(I've already been down the Church-encoded numbers road.)

## Notes

While inspired by Kernel, this interpreter is making some different choices.

This interpreter does not make a distinction between operatives and
applicatives. An applicative is merely an operative that chooses to evaluate
its arguments, and the interpreter cannot distinguish between the two.

Kernel provides `wrap` as a primitive, but this interpreter instead provides
`invoke` as a primitive. While Kernel's `apply` applies applicatives, our
`invoke` invokes operatives. (And I'm not yet sure of the implications of not
having an `unwrap` primitive.)

The `#skip` and `#end` tokens can be used, much like `#if` and `#endif` in C,
to skip over code to temporarily disable it.

While noeval does support varidic parameters, it only supports getting all the
arguments as a single list.

    (vau args env body)

It does not support the pair or improper list syntax for a combination of
fixed parameters with a rest parameter.

    (vau (param1 param2 . rest) env body)

Several times, I almost changed...

    (vau (operands) env body) -> (fexpr env (operands) body)

...but always landed on keeping with convention.

In fact, the language does not fully support pairs or improper lists.
(I expect at some point to try to move towards replacing lists with arrays.)

Both `vau` and `lambda` (which is in the library) only support a single
expression for the body.

Noeval's `do` is the equivalent of CL `progn`, Scheme `begin`, and Kernel's
`$sequence`.

Noeval uses Church booleans, which allows not having a primitive conditional.
Arguably that's signing up for more overhead, but... Unlike Church numbers, I'm
not sure that overhead will end up mattering for my purposes. I also wonder if
code written with Church booleans in mind might minimize the overhead. Anyway,
that's the choice I've made for now.

The `true` and `false` Church booleans are added to the global environment, and
they're tagged internally so that they'll print as names instead of using the
usual print form of operatives.

This means that, at the REPL, true and false results look like this:

    noeval> (nil? ())
    => (operative (x y) env (eval x env))
    noeval> (nil? (list 1 2 3))
    => (operative (x y) env (eval y env))

Unlike Kernel, all objects (or perhaps more precisely, all bindings) are
immutable by default. The `define-mutable` form can be used to create a mutable
binding, which can then be modified with the `set!` primitive.

Kernel uses an interesting trick with `unwrap` and `define` to implement `set!`
by modifying the dynamic environment. I'm not sure whether I think this is a
good idea or not.

There's currently no symbolic form of `()`.

There's currently no equivalent to Kernel's `#ignore`. The `vau` operative,
however, will treat `()` as the environment parameter name the way Kernel
treats `#ignore`.

Currently the only equivalence predicate is `=`, which may be closest to
Kernel's `equal?`. It only works for numbers, symbols, strings, and `()`.

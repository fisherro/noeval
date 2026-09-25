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

## Building and running

Noeval needs:

* GCC 14 or later (it uses C++26 features)
* Boost (only the header-only Multiprecision library)
* GNU Readline

Build with `make`. The executable is `bin/noeval`.

The Makefile doesn't enable optimization. For a much faster interpreter:

    make clean
    make CXXFLAGS='-std=c++26 -O2'

Run it from the top of the repository, since it loads `src/lib.noeval` and
`tests/main.noeval` by relative path.

* `bin/noeval` runs the C++ tests, loads the library, and starts the REPL.
  (Type `:help` in the REPL for its commands.)
* `bin/noeval script.noeval` runs the tests, loads the library, and then runs
  the script instead of starting the REPL. It exits with a failure status if
  the script raises an error.
* `bin/noeval --tests` runs the C++ tests and the library tests, then exits
  with a status reflecting the results.
* `bin/noeval --gc-tests` runs only the garbage collection tests.

The library tests take a while, so they don't run at startup. Use `--tests`, or
`:reload` in the REPL to reload the library and run them.

If the C++ tests fail at startup, noeval asks whether to continue. It only asks
when stdin is a terminal. Otherwise it exits with a failure status.

Setting `NOEVAL_GC_STRESS=n` makes the garbage collector run every `n`
environment creations, which is useful for finding GC bugs. See
[design/env-gc.md](design/env-gc.md).

## Notes

While inspired by Kernel, this interpreter is making some different choices.

### Wrap & unwrap are not primitives

This interpreter does not make a distinction between operatives and
applicatives. An applicative is merely an operative that chooses to evaluate
its arguments, and the interpreter cannot distinguish between the two.

Kernel provides `wrap` as a primitive, but this interpreter instead provides
`invoke` as a primitive. While Kernel's `apply` applies applicatives, our
`invoke` invokes operatives. (And I'm not yet sure of the implications of not
having an `unwrap` primitive.)

In hindsight, I don't think this was a good choice. I was thinking that having
the distinction between operatives and applicatives would require separate
types, but really it could just be a `bool` on the existing operative type.

The `wrap` and `unwrap` primitives would then be trivial. The code to evaluate
operatives could check the flag and decide whether to evaluate the arguments or
not.

Then we could have `operative?`, `applicative?`, and `callable?` predicates,
which might be useful.

But this kind of insight was exactly the purpose of this project. If there's
ever a Noeval 2, I'll probably go the other way on this.

### Disabling code

The `#skip` and `#end` tokens can be used, much like `#if` and `#endif` in C,
to skip over code to temporarily disable it.

### Variadic combiners

While noeval does support varidic parameters, it only supports getting all the
arguments as a single list.

    (vau args env body)

It does not support the pair or improper list syntax for a combination of
fixed parameters with a rest parameter.

    (vau (param1 param2 . rest) env body) ; not supported

In fact, the language does not fully support pairs or improper lists.
(I expect at some point to try to move towards replacing lists with arrays.)

### Single expression bodies for vau & lambda

Both `vau` and `lambda` (which is in the library) only support a single
expression for the body.

The library provides `vau*` and `lambda*` that support multiexpression bodies.

### The syntax of vau

Several times, I almost changed...

    (vau (operands) env body) -> (fexpr env (operands) body)

...but always landed on keeping with convention.

### do

Noeval's `do` is the equivalent of CL `progn`, Scheme `begin`, and Kernel's
`$sequence`.

### Church Booleans

Noeval uses Church Booleans, which allows not having a primitive conditional.
Arguably that's signing up for more overhead, but... Unlike Church numbers, I'm
not sure that overhead will end up mattering for my purposes. I also wonder if
code written with Church Booleans in mind might minimize the overhead. Anyway,
that's the choice I've made for now.

The `true` and `false` Church Booleans are added to the global environment, and
they're tagged internally so that they'll print as names instead of using the
usual print form of operatives.

Because Noeval uses Church Booleans, other values do not represent truthiness
or falsiness.

### Limited mutability

Unlike Kernel, all objects (or perhaps more precisely, all bindings) are
immutable by default. The `define-mutable` form can be used to create a mutable
binding, which can then be modified with the `set!` primitive.

Kernel uses an interesting trick with `unwrap` and `define` to implement `set!`
by modifying the dynamic environment. I'm not sure whether I think this is a
good idea or not.

### Nil is spelt ()

There's currently no symbolic form of `()`.

### No #ignore or #inert

There's currently no equivalent to Kernel's `#ignore`. The `vau` operative,
however, will treat `()` as the environment parameter name the way Kernel
treats `#ignore`.

### There is only =

Currently the only equivalence predicate is `=`, which may be closest to
Kernel's `equal?`. It raises an error when comparing different types (with a
few exceptions)

### Numbers

All numbers are arbitrary precision rationals.

### Garbage collection

Values and environments are reference counted, with a cycle collector for
environments. Cycles are common: a closure captures its whole environment, so
defining a local function puts a closure into the environment it captures.

Fexprs don't create most of these cycles, but they do make them harder to
avoid. Techniques like flat closures and lambda lifting rely on knowing which
variables a function body uses, and that can't be known when any code might be
evaluated in any environment. See
[design/gc-postmortem.md](design/gc-postmortem.md) for the analysis and
[design/env-gc.md](design/env-gc.md) for how the collector works.

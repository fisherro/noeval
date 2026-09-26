# Primitives

A survey of the builtins, asking which could move to the library (see
"Minimize primitives" in [CONTRIBUTING.md](../CONTRIBUTING.md)). Candidates
were tried by removing the builtin and defining it in `src/lib.noeval`, then
running the tests and counting instructions with cachegrind on a few
benchmarks. The counts are relative to the builtin version.

## Summary

| Builtin | Verdict |
| --- | --- |
| `invoke` | Moved to the library, at no measurable cost |
| `numerator` | Keep: it could move, but there's nothing to gain |
| `nil?` | Can move, but costs up to 17% on list-heavy code |
| `do` | Keep: a library version runs about 3 times as many instructions |
| `write`, `display` | Keep for now; see the TODO item about consolidating them |
| Everything else | Keep: needed, or not practical in the library |

## Candidates

### invoke

`(invoke operative arg-list)` built `(operative . arg-list)` and evaluated it
in the calling environment, which the library can do directly, so it's now
defined there:

```scheme
(define invoke
  (vau (operative operands) env
    (eval (cons operative (eval operands env)) env)))
```

The instruction counts didn't change (within 0.2%), since only `prepend`
uses `invoke`. Its C++ tests moved to the library tests, since the C++ tests
run without the library.

`invoke` is close to `apply`. Now that it's in the library, it could also be
dropped, with `prepend` using `eval` and `cons` itself.

### numerator

Nothing in the library or the library tests uses `numerator`, and for any
number `x`, `(numerator x)` is `(* x (denominator x))`. That was checked for
0, integers, and negative and positive fractions. `denominator` stays, since
`numerator` can't give it back without dividing, and `integer?` uses it.

But `numerator` stays a builtin too. It's trivial for the interpreter, and
moving it gains no flexibility, so it meets neither of the reasons for moving
something to the library that "Minimize primitives" in
[CONTRIBUTING.md](../CONTRIBUTING.md) gives: a significant simplification of
the interpreter, or more expressive library code.

### nil?

`=` treats nil specially before it compares types, so `(= x ())` never
raises and is exactly `nil?`:

```scheme
(define nil? (vau (x) env (= (eval x env) ())))
```

The library tests pass, but every list loop calls `nil?`, and each call to the
library version creates an environment:

| Benchmark | Instructions |
| --- | --- |
| `lists` | +9% |
| `string-index` | +17% |
| `library-tests` | +5% |
| `startup` | +5% |

That seems too much for a small saving. The C++ tests would also need changes,
since 8 of them use `nil?` without the library.

### do

A library `do` has to evaluate its last expression as a tail call, as the
builtin does, or loops written with `lambda*` would grow the stack. The
version that was in `src/lib.noeval` (skipped with `#skip`) didn't. This one
does, using `cons` to sequence two evaluations, since there's no `do` yet to
sequence them:

```scheme
(define do
  (vau expressions env
    ((nil? expressions)
     ()
     ((nil? (rest expressions))
      (eval (first expressions) env)
      (eval (cons do (rest expressions))
            (first (rest (cons (eval (first expressions) env)
                               (cons env ())))))))))
```

The library tests pass and a 100,000-iteration loop runs, but every body of a
`lambda*`, `vau*`, `when`, `unless`, and `cond` clause goes through `do`:

| Benchmark | Instructions |
| --- | --- |
| `fib` | 2.9× |
| `lists` | 3.3× |
| `let-when-unless` | 3.6× |
| `library-tests` | 3.2× |
| `startup` | 1.6× |

Errors still report the same locations. (Eight C++ tests fail, but only
because they use `do` without loading the library.) So `do` stays a builtin,
for speed. The skipped library version, which didn't make tail calls, and the
`USE_PRIMITIVE_DO` switch that went with it have been removed, with this
section as the record of why.

### write and display

The library doesn't use `write`, and `display` differs from it only for
strings, which it prints without quotes or escapes. Neither can move to the
library as they are, since printing an operative or an environment needs the
interpreter's printer. The existing TODO item, to consolidate them into one
primitive, is the way to reduce them: for example, a primitive that returns
the printed form of a value as a string, and one that prints a string as it
is, with both `write` and `display` in the library.

## Keep

- **Core evaluation**: `vau`, `eval`, `define`, `macro` (the expansion cache
  can't be kept by the library; see [macros.md](macros.md)), `try`, `raise`,
  and the Church Booleans `true` and `false`, which the builtins return.
- **`eval-list`**: it used to be in the library, but its helper created a
  cycle on every call to a wrapped operative (see the comment on
  `eval_list_operative`).
- **Data**: `cons`, `first`, `rest`, `=`, `typeof`, `string->list`,
  `list->string`, `string->symbol`, `symbol->string`.
- **Numbers**: `+`, `-`, `*`, `/`, `<=>`, `numerator` (see above),
  `denominator`, and `remainder`, which `quotient` and `modulo` are built on.
  (Integer division can't be made from the others without looping.)
  `number->string` and `string->number` could be written in the library, but
  they read and write the same forms as the reader and printer, which they
  share code with.
- **Mutation**: `define-mutable`, `set!`.
- **I/O**: `load`, `read`, `flush`.
- **Environments**: `environment-names`, `environment-parent`, and the
  `get-builtins-environment`, `get-library-environment`, and
  `get-top-level-environment` getters, which reach environments no code can
  otherwise refer to.

# Noeval Language Reference

A summary of the language, for working on Noeval code (for example, as context for an AI agent) without reading large amounts of interpreter code.

## Built-in Operatives (C++)

**Control**: `vau`, `eval`, `eval-list`, `define`, `invoke`, `do`, `try`, `raise`, `load`, `macro` (see [Macros](#macros))
**Arithmetic**: `+`, `-`, `*`, `/` (evaluate all arguments). As in Scheme: `(+)` is 0 and `(*)` is 1, while `(-)` and `(/)` are errors; `(- x)` negates `x` and `(/ x)` is its reciprocal; with more arguments, the operator is applied from the left, so `(- 10 3 2)` is 5.
**Numeric operations**: `numerator`, `denominator`, `remainder`
**Numeric comparisons**: `<=>` (evaluate all arguments)
**Lists**: `cons`, `first`, `rest`, `nil?` (evaluate all arguments)
**Strings**: `string->list` and `list->string` convert to/from lists of Unicode codepoints as Noeval numbers
**Symbols**: `string->symbol` returns the symbol with a string as its name (any string, even one that wouldn't read back as that symbol), and `symbol->string` returns a symbol's name
**Numbers and strings**: `number->string` and `string->number`. `(number->string number [radix] [style])` writes a number in a radix from 2 to 36 (10 by default, with lowercase letters for digits above 9) in a style: `:decimal` (repeating digits in parentheses, as in `0.1(6)`), `:fraction` (`1/6`), or `:auto` (the default, which is how numbers print: a decimal if it terminates and a fraction otherwise). The radix and style can be given in either order. `(string->number string [radix])` reads any form `number->string` writes, with letters in either case, and returns `()` for a string that isn't a number.
**Predicates**: `=` (evaluate all arguments)
**I/O**: `write`, `display`, `flush` (evaluate all arguments), `read` (reads the next expression from standard input, or returns an eof object at the end)
**Mutation**: `define-mutable`, `set!`
**REPL only**: `redefine` (like `define`, but may rebind a name that's already bound)
**Church Booleans**: `true`, `false` (built-in operatives)
**Reflection**: `typeof`
**Environments**: `environment-names`, `environment-parent`, `get-builtins-environment`, `get-top-level-environment` (see [Environments](#environments))

## Key Language Patterns

- **Church Booleans**: `((condition) true-branch false-branch)`
- **Variadic parameters**: `vau` and `lambda` support variadic parameters using single symbol form `(lambda args ...)` but not dotted pair form `(lambda (first . rest) ...)`
- **Single vs multiple expressions**: `lambda` and `vau` support only single body expressions; use `lambda*` and `vau*` for multiple expressions in the body
- **Lists**: Built from `cons` cells, terminated with `()`
- **Unevaluated arguments**: Operatives receive raw expressions
- **Evaluation rule**: As in Kernel, a symbol is looked up and a cons cell is a combination; every other value (numbers, strings, `()`, operatives, macros, environments, the eof object) evaluates to itself. So code built at runtime can contain any value, not just its name.
- **Environment access**: Second parameter to `vau` gets calling environment
- **Nil representation**: `()` not `nil`
- **Mutation restrictions**: Only variables created with `define-mutable` can be modified with `set!` - attempting to `set!` a variable created with `define` will raise an error
- **Environment transparency**: `do` and `try` do not create new environments - definitions made within them persist in the current environment
- **Numbers**: Arbitrary precision rationals (fractions) - all arithmetic preserves exact precision. A number prints as a decimal when its decimal terminates and as a fraction otherwise: `1/4` prints as `0.25` and `1/3` as `1/3`.
- **Keywords**: By convention, a symbol whose name starts with `:`, such as `:auto`, is a keyword, and is defined to evaluate to itself so that it can be passed as an option without quoting. `(define-keyword :name ...)` (a macro) defines keywords, and `keyword?` tests for one. Declaring a keyword more than once is fine: `define-keyword` does nothing for a keyword already bound to itself, in the calling environment or an enclosing one, but raises an error for one bound to anything else. `define` still can't rebind a keyword.
- **Rational decomposition**: `numerator` and `denominator` extract parts of fractions
- **Error handling**: `try` catches exceptions and passes them to handler as error lists with structure `(error message context stack-trace)`
- **Testing**: `test-assert` and `test-error` for writing tests; test results tracked globally

## Standard Library (lib.noeval)

**Core**: `lambda` (single expression), `lambda*` (multiple expressions), `vau*` (multiple expressions), `wrap`, `apply`, and the macros `if`, `let` and `cond` (see [Macros](#macros))
**Lists**: `append`, `reverse`, `length`, `filter`, `map`, `foldl`, `foldr`, `foldl-until`, `foldr-until`, `unfoldl`, `unfoldr`, `last`, `list`, `snoc`, `iota`, `prepend`, `second`, `third`, `fourth`, `nth` (`(nth list index)`, zero-based), `take` and `drop` (`(take n list)`), `list-index`, `any?` and `all?` (`(any? predicate list)`)
**Control**: `when` and `unless` (macros)
**Logic**: `and` and `or` (macros), and `not` (an operative). `and` and `or` short-circuit, and treat every operand, including the last, as a Church Boolean, so a non-Boolean operand is an error. `nand` and `nor` are macros: `(nand x ...)` is `(not (and x ...))` and `(nor x ...)` is `(not (or x ...))`, so they short-circuit the same way. `xor` and `xnor` are functions, which evaluate every operand: `xor` is true when an odd number of its operands are true, so `(xor a b c)` is `(xor (xor a b) c)`, and `(xnor ...)` is `(not (xor ...))`. `(xor)` is false and `(xnor)` is true.
**Pipelines**: `pipe` and `pipe-it` (macros) pass a value through a sequence of expressions: `(pipe x 10 (+ x 1) (* x 2))` binds `x` to each value in turn and is 22, like nested `let`s. `(pipe-it expression ...)` is `(pipe it expression ...)`, binding `it` on purpose.
**Infix**: `infix` (a macro) allows infix notation, with parentheses for grouping and no precedence: `(infix (1 + 2) * (10 - 4))` is `(* (+ 1 2) (- 10 4))`. An expression has one element (a value, or a parenthesized expression), two (`op x`, which is `(op x)`), or three (`x op y`, which is `(op x y)`).
**Keywords**: `define-keyword` (a macro): `(define-keyword :name ...)` defines each keyword to evaluate to itself, does nothing for one already bound to itself, and raises an error for one bound to anything else or for a name that doesn't start with `:` (see Key Language Patterns)
**Predicates**: `odd?`, `even?`, `number?`, `integer?`, `non-negative-integer?`, `string?`, `symbol?`, `keyword?`, `list?`, `operative?`, `macro?`, `environment?`, `eof-object?`
**I/O**: `newline`, `displayln`, `lndisplayln`, `for-each`
**Meta**: `q`, `get-current-environment`, `unevaluated-list`, `gensym` (`(gensym [prefix])` returns a new symbol, `#:g1`, `#:g2`, and so on, for a macro to bind without capturing the user's names; a prefix, a string or symbol, replaces the `g`)
**Partial application**: `partiall` and `partialr` fix the leftmost or rightmost arguments: `((partiall - 10) 3)` is 7, `((partialr - 10) 3)` is -7. `partiall-lazy` and `partialr-lazy` don't evaluate the fixed arguments until the resulting function is called, and evaluate them on every call.
**Validation**: `check` (`(check value predicate message)` returns `value` if `(predicate value)` is true and raises `message` otherwise)
**Examples**: `countdown`, `factorial`, `fibonacci`
**Comparisons**: `!=`, `<>` (alias for `!=`)
**Numeric comparisons**: `<`, `>`, `<=`, `>=` (with Unicode aliases `≤`, `≥`)
**Numeric operations**: `abs`, `modulo`, `quotient` (integer division), `clamp` (`(clamp value lower higher)`)
**String operations**: `string-length`, `string-nth`, `substring`, `string-append`, `strings->string`, `string->codepoint-strings`, `codepoints->utf8` and `utf8->codepoints` (convert between lists of codepoints and lists of UTF-8 byte values)
**Testing**: `test-assert`, `test-error` (for library test suite)
**Internal helpers**: `cond-transformer` (`cond`'s transformer)
**Unicode support**: `λ` (alias for `lambda`), `∧` (alias for `and`), `∨` (alias for `or`), `¬` (alias for `not`), `⊼` (alias for `nand`), `⊽` (alias for `nor`), `⊻` (alias for `xor`), `×` (alias for `*`), `÷` (alias for `/`)

## Environments

Environments are first-class values that can be inspected.

- **Builtins and the top level**: The builtins are in their own environment. Its child, the top-level environment, is where the library is loaded, and the REPL and scripts evaluate in it too. A definition at the top level can shadow a builtin but doesn't replace it.
- **get-builtins-environment**, **get-top-level-environment**: `(get-builtins-environment)` and `(get-top-level-environment)` return those environments
- **environment-names**: `(environment-names env)` returns a sorted list of the symbols bound in `env` itself, not in its ancestors
- **environment-parent**: `(environment-parent env)` returns `env`'s parent environment, or `()` for the builtins environment

## Macros

`(macro operative)` returns a macro (`typeof` gives `macro`). When a macro is the operator of a combination, its transformer (the operative) is called with the unevaluated operands, and the result (the expansion) is evaluated in the calling environment. See `design/macros.md`.

- **Write transformers with `vau`**: a transformer receives the operands unevaluated, so a `lambda` would evaluate them.
- **Empty environment**: the transformer's environment argument is a new environment with no bindings and no parent, so an expansion can depend only on the operands.
- **Embed values, not names**: build expansions with `list` and `cons` so that they contain the values of `if`, `do` and so on, not their names, which the calling environment might rebind: `(macro (vau args _ (list if (first args) (cons do (rest args)) ())))`.
- **Capture on purpose with names**: hygiene is a convention, chosen name by name. A symbol in an expansion is looked up, or bound, in the calling environment, so a macro can deliberately insert a name instead of a value. This supports anaphoric macros, which bind a name such as `it` for their body (`(cons let (cons (list (list (q it) (first args))) (rest args)))`), macros that refer to a caller's variable by name, and defining macros, which `define` names given as operands or made from them with `string->symbol`. A captured name is looked up wherever the combination is evaluated, even though the expansion is cached.
- **No new scope**: the expansion is evaluated in the calling environment, so a `define` in it binds there, and `define`'s usual rule against rebinding applies.
- **Values anywhere**: every value except a symbol or a cons cell evaluates to itself, so an expansion can contain an operative, macro, or environment in any position, not just as an operator: `(list define name first)` defines `name` as `first`'s value.
- **Expanded once**: a combination's expansion is cached, invisibly, on the combination, and reused each time the combination is evaluated with a macro that has the same transformer. So a transformer runs once per combination, not once per call, and it shouldn't have side effects. A combination built at runtime (`(eval (cons m args) env)`) is expanded each time it's evaluated.

## Error Handling

- **Exception structure**: `(error message context stack-trace)`
- **try syntax**: `(try expr handler)` or `(try expr handler finally)`
- **raise syntax**: `(raise message)` - creates evaluation_error with message
- **Error propagation**: Evaluation errors are re-thrown unchanged; other exceptions are wrapped
- **Source locations**: Errors are reported with the `file:line:column` of the innermost expression being evaluated that was read from a file. The stack trace shows each frame's location, and the expression most recently reached by a tail call from that frame. The `message` in the error list doesn't include the location.

## Loading Files

- **load syntax**: `(load filename)` - evaluates each expression in the file in the current environment and returns the value of the last one
- **Relative paths**: While a file is being loaded, a relative `filename` is resolved against that file's directory. Otherwise, such as at the REPL, it is resolved against the current directory. Scripts given on the command line are loaded the same way, so a script can load the files next to it by name.

## Testing Framework

- **test-assert**: `(test-assert condition message)` - evaluates condition, reports pass/fail
- **test-error**: `(test-error expr message)` - expects expr to throw an error
- **Global tracking**: `test-failures` and `test-count` track test results
- **Output formatting**: Colored output with ✓/✗ indicators

## Parser Features

- **Comments**: `;` to end of line
- **Conditional compilation**: `#skip` and `#end` blocks to disable code sections
- **String literals**: Support standard escape sequences
- **Numeric literals**: Supports rationals (e.g., `1/3`), decimals (e.g., `0.5`), and various bases (`#x10`, `#b1010`, `#o17`, `#16rAF`)

## Implementation Notes

- **Tail call optimization**: Enabled with `USE_TAIL_CALL`
- **Garbage collection**: Reference counting plus a cycle collector for environments, which runs automatically as environments are created (see `design/env-gc.md`)
- **Debug categories**: `eval`, `builtin`, `env_binding`, `tco`, `timer`, `library`, `macro` (expansions and cache hits)
- **Call stack tracking**: Maintains call stack for error reporting
- **Environment chaining**: Environments form chains for lexical scoping

## Common Pitfalls

### Mutation Errors

- `(define x 42)` creates an **immutable** binding - `(set! x 99)` will fail
- `(define-mutable x 42)` creates a **mutable** binding - `(set! x 99)` will succeed
- `set!` can only modify variables that were explicitly created as mutable
- `define` and `define-mutable` can't rebind a name already bound in the same environment, mutable or not. Use `set!` for a mutable binding or `let` for a new scope. Shadowing a name from an enclosing environment is allowed.
- In the REPL, `redefine` can rebind a name

### Body Expression Limits

- `lambda` takes exactly one body expression: `(lambda (x) (+ x 1))` ✓
- For multiple expressions, use `lambda*`: `(lambda* (x) (displayln x) (+ x 1))` ✓  
- Same applies to `vau` vs `vau*` for operatives
- Multiple expressions without `*` forms will cause syntax errors

### do and try Don't Create Scopes

- `do` and `try` evaluate in the current environment, so definitions made inside them remain afterward: after `(do (define x 1))`, `x` is bound
- The same applies to `when`, `unless`, and `cond` clause bodies, which are wrapped in `do`
- A definition made in a `try`'s body before an error remains after the error is handled
- For a new scope, use `let` or `lambda*`: `(let () (define x 1) x)` doesn't bind `x` outside

### Church Boolean Usage

- Conditions return operatives, not boolean values
- Use `((condition) true-branch false-branch)` pattern
- `and`, `or`, `not` work with Church Booleans, not primitive booleans

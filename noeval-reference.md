# Noeval Language Reference

Summary to use as Github Copilot context so that it doesn't have to reference large amounts of interpreter code when working on code in the Noeval language itself.

## Built-in Operatives (C++)

**Control**: `vau`, `eval`, `define`, `invoke`, `do`, `try`, `raise`
**Arithmetic**: `+`, `-`, `*`, `/` (evaluate all arguments)
**Numeric operations**: `numerator`, `denominator`, `remainder`
**Numeric comparisons**: `<=>` (evaluate all arguments)
**Lists**: `cons`, `first`, `rest`, `nil?` (evaluate all arguments)
**Strings**: `string->list` and `list->string` convert to/from lists of Unicode codepoints as Noeval numbers
**Predicates**: `=` (evaluate all arguments)
**I/O**: `write`, `display`, `flush` (evaluate all arguments)
**Mutation**: `define-mutable`, `set!`
**Church Booleans**: `true`, `false` (built-in operatives)
**Reflection**: `typeof`

## Key Language Patterns

- **Church Booleans**: `((condition) true-branch false-branch)`
- **Variadic parameters**: `vau` and `lambda` support variadic parameters using single symbol form `(lambda args ...)` but not dotted pair form `(lambda (first . rest) ...)`
- **Single vs multiple expressions**: `lambda` and `vau` support only single body expressions; use `lambda*` and `vau*` for multiple expressions in the body
- **Lists**: Built from `cons` cells, terminated with `()`
- **Unevaluated arguments**: Operatives receive raw expressions
- **Environment access**: Second parameter to `vau` gets calling environment
- **Nil representation**: `()` not `nil`
- **Mutation restrictions**: Only variables created with `define-mutable` can be modified with `set!` - attempting to `set!` a variable created with `define` will raise an error
- **Environment transparency**: `do` and `try` do not create new environments - definitions made within them persist in the current environment
- **Numbers**: Arbitrary precision rationals (fractions) - all arithmetic preserves exact precision
- **Rational decomposition**: `numerator` and `denominator` extract parts of fractions
- **Error handling**: `try` catches exceptions and passes them to handler as error lists with structure `(error message context stack-trace)`
- **Testing**: `test-assert` and `test-error` for writing tests; test results tracked globally

## Standard Library (lib.noeval)

**Core**: `lambda` (single expression), `lambda*` (multiple expressions), `vau*` (multiple expressions), `wrap`, `apply`, `if`, `let`, `cond`
**Lists**: `append`, `reverse`, `length`, `filter`, `map`, `foldl`, `foldr`, `list`, `snoc`, `iota`, `prepend`, `second`, `list-ref`, `list-index`
**Control**: `when`, `unless`, `and`, `or`, `not`
**Predicates**: `odd?`, `even?`, `number?`, `integer?`, `string?`, `symbol?`, `list?`, `operative?`, `environment?`
**I/O**: `newline`, `displayln`, `lndisplayln`, `for-each`
**Meta**: `q`, `get-current-environment`, `unevaluated-list`, `eval-list`
**Examples**: `countdown`, `factorial`
**Comparisons**: `!=`, `<>` (alias for `!=`)
**Numeric comparisons**: `<`, `>`, `<=`, `>=` (with Unicode aliases `≤`, `≥`)
**Numeric operations**: `abs`, `modulo`
**String operations**: `string-length`, `string-nth`, `substring`, `string-append`, `strings->string`, `string->codepoint-strings`
**Testing**: `test-assert`, `test-error` (for library test suite)
**Unicode support**: `λ` (alias for `lambda`), `∧` (alias for `and`), `∨` (alias for `or`), `¬` (alias for `not`), `×` (alias for `*`), `÷` (alias for `/`)

## Error Handling

- **Exception structure**: `(error message context stack-trace)`
- **try syntax**: `(try expr handler)` or `(try expr handler finally)`
- **raise syntax**: `(raise message)` - creates evaluation_error with message
- **Error propagation**: Evaluation errors are re-thrown unchanged; other exceptions are wrapped

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
- **Garbage collection**: Manual environment collection after top-level evaluation
- **Debug categories**: `eval`, `builtin`, `env_binding`, `tco`, `timer`, `library`
- **Call stack tracking**: Maintains call stack for error reporting
- **Environment chaining**: Environments form chains for lexical scoping

## Common Pitfalls

### Mutation Errors
- `(define x 42)` creates an **immutable** binding - `(set! x 99)` will fail
- `(define-mutable x 42)` creates a **mutable** binding - `(set! x 99)` will succeed
- `set!` can only modify variables that were explicitly created as mutable

### Body Expression Limits
- `lambda` takes exactly one body expression: `(lambda (x) (+ x 1))` ✓
- For multiple expressions, use `lambda*`: `(lambda* (x) (displayln x) (+ x 1))` ✓  
- Same applies to `vau` vs `vau*` for operatives
- Multiple expressions without `*` forms will cause syntax errors

### Church Boolean Usage
- Conditions return operatives, not boolean values
- Use `((condition) true-branch false-branch)` pattern
- `and`, `or`, `not` work with Church Booleans, not primitive booleans

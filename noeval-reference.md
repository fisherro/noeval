# Noeval Language Reference

Summary to use as Github Copilot context so that it doesn't have to reference large amounts of interpreter code when working on code in the Noeval language itself.

## Built-in Operatives (C++)

**Control**: `vau`, `eval`, `define`, `invoke`, `do`, `try`, `raise`
**Arithmetic**: `+`, `-`, `*`, `/` (evaluate all arguments)
**Lists**: `cons`, `first`, `rest`, `nil?` (evaluate all arguments)
**Predicates**: `=` (evaluate all arguments)
**I/O**: `write`, `display` (evaluate all arguments)
**Mutation**: `define-mutable`, `set!`
**Church Booleans**: `true`, `false` (built-in operatives)
**Reflection**: `typeof`

## Key Language Patterns

- **Church Booleans**: `((condition) true-branch false-branch)`
- **Variadic parameters**: `vau` and `lambda` support variadic parameters using single symbol form `(lambda args ...)` but not dotted pair form `(lambda (first . rest) ...)`
- **Multiple body expressions**: Use `vau*` and `lambda*` for multiple expressions
- **Unevaluated arguments**: Operatives receive raw expressions
- **Environment access**: Second parameter to `vau` gets calling environment
- **Nil representation**: `()` not `nil`
- **Lists**: Built from `cons` cells, terminated with `()`

## Standard Library (lib.noeval)

**Core**: `lambda`, `lambda*`, `vau*`, `wrap`, `apply`, `if`, `let`, `cond`
**Lists**: `append`, `reverse`, `length`, `filter`, `map`, `foldl`, `foldr`, `list`, `snoc`, `iota`, `prepend`, `second`
**Control**: `when`, `and`, `or`, `not`
**Predicates**: `odd?`, `even?`, `number?`, `string?`, `symbol?`, `list?`, `operative?`, `environment?`
**I/O**: `newline`, `displayln`, `for-each`
**Meta**: `q`, `get-current-environment`, `unevaluated-list`, `eval-list`
**Examples**: `countdown`, `factorial`

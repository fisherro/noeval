# Style

## C++

- **snake_case** for variables, functions, and types (following standard
  library conventions)
- **Cuddled braces** for `catch` and `else`
- **Function braces on a new line** (but not for other constructs)
- **Space after keywords** (`if (`, `while (`) but not after function names
  in calls (`func(`)
- **Logical keywords**: prefer `and`, `or`, and `not` over `&&`, `||`, and
  `!`
- **`auto` and abbreviated function templates** are preferred when type names
  aren't needed
- **Colon spacing**: no space before, a space after (conditionals,
  initializer lists)
- **Constant first for equality checks**: write `0 == x` instead of `x == 0`,
  to prevent accidental assignment (`x = 0`). This applies only to `==` and
  `!=`, not to ordering comparisons (`<`, `>`, `<=`, `>=`).
- **Trailing commas**: include a trailing comma in lists wherever it is
  allowed, even after the last element

## Noeval

- `_` names an ignored parameter, such as an operative's unused environment
  parameter: `(vau (operand) _ operand)`.
- Nil is spelt `()`.
- Avoid `q` (quote) where it isn't needed. In a fexpr-based language, an
  operative can take its operands unevaluated instead.
- Comments start with `;`. To disable a block of code temporarily, wrap it in
  `#skip` and `#end`.

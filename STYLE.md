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
- **Trailing commas**: in a list that spans multiple lines, include a
  trailing comma after the last element wherever it is allowed. A list on a
  single line, like `{a, b}`, doesn't get one.

## Noeval

- `_` names an ignored parameter, such as an operative's unused environment
  parameter: `(vau (operand) _ operand)`.
- Nil is spelt `()`.
- Avoid `q` (quote) where it isn't needed. In a fexpr-based language, an
  operative can take its operands unevaluated instead.
- Name an option that's passed as a symbol with a keyword: a symbol starting
  with `:`, such as `:auto`, defined with `define-keyword` so that it
  evaluates to itself and doesn't need quoting. The `:` also keeps keywords
  from colliding with names users define.
- Comments start with `;`. To disable a block of code temporarily, wrap it in
  `#skip` and `#end`.

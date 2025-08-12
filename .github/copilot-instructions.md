# AI Coding Agent Instructions for Noeval

## Working Approach
- **Don't make assumptions or jump to conclusions** - Take time to analyze fully and come up with the correct answer, or ask clarifying questions or suggest tests to determine missing information
- **Verify before asking** - Don't ask a question or suggest a test until you've confirmed you don't already have access to the needed information
- **Aim for accuracy and completeness** - Take time to provide thorough, correct answers on the first attempt

## Project Overview
Noeval is a **fexpr-based** Lisp interpreter inspired by John Shutt's Kernel language. The key architectural insight: operatives (fexprs) receive **unevaluated** arguments and have access to the calling environment, enabling powerful metaprogramming without traditional macros.

## Core Architectural Principles

### Fexpr-Based Evaluation
- **Operatives control evaluation** - they receive unevaluated arguments and decide what to evaluate when
- **Environment access** - operatives get the calling environment as a parameter
- **Church Booleans** - conditionals work by direct function application, no primitive `if` needed
- **Immutable by default** - mutation is explicit and controlled

### Value System Architecture
The interpreter uses a variant-based value system with these key types:
- Primitive values (int, string, symbol)  
- Composite structures (cons cells for lists)
- Executable types (operatives and built-in operatives)
- Environment and mutable binding abstractions

## Development Patterns

### Build & Test Workflow
- **Always use containerized builds** - the project has custom container setup, never use raw `g++`
- **Dual testing strategy** - C++ unit tests run first, then language-level tests
- **Use VS Code tasks** for primary development workflow
- **C++26 standard** - Most C++23 and many C++26 features are available

### Language Design Philosophy
- **Minimize primitives** - implement as much as possible in the language itself
- **Explicit evaluation** - when implementing operatives, be intentional about what gets evaluated when
- **Church Boolean patterns** - conditions return functions that select between alternatives
- **Error context** - always provide meaningful error messages with expression context

### Code Organization
- Core interpreter in main source files
- Standard library in `.noeval` files  
- Built-ins follow consistent patterns: validate arguments → evaluate selectively → return value
- Debug infrastructure supports category-based logging

## Common Patterns & Pitfalls

### Working with Operatives
- Operatives get **unevaluated** arguments - don't assume they're values
- Environment parameter provides access to calling context
- Use `()` as environment parameter name to ignore it
- Church Booleans use calling convention: `(condition true-branch false-branch)`

### Error Handling
- Re-throw `evaluation_error` instances unchanged
- Wrap other exceptions with context
- Use `expr_context()` for readable error messages
- Provide specific error messages, not generic ones

### C++ Style Conventions
- **snake_case** for variables, functions, types (following standard library conventions)
- **Cuddled braces** for catches and else statements
- **Function braces on new line** (but not for other constructs)  
- **Space after keywords** (`if (`, `while (`) but not function calls (`func(`)
- **Use logical keywords** (`and`, `or`, `not`) instead of operators (`&&`, `||`, `!`)
- **Auto/abbreviated templates** preferred when type names aren't needed
- **Colon spacing**: no space before, space after (conditionals, initializer lists)

### Parser Features  
- Comments use `;` to end of line
- `#skip`/`#end` blocks for conditional compilation/debugging
- String literals support standard escape sequences

## Testing Philosophy
When implementing features:
- Add C++ tests for low-level functionality
- Add language tests for user-visible behavior
- Test error conditions with meaningful messages
- Verify operatives work with unevaluated arguments

The goal is a minimal yet powerful interpreter where complex behaviors emerge from simple, composable primitives.

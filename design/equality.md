# Equality notes

Robert-Fisher_tmemu: Summarize the difference, in Scheme, between `=`, `eq?`, `eqv?`, and `equal?`.

GitHub Copilot: In Scheme, there are four different equality predicates with increasingly liberal definitions of "sameness":

## `=` - Numeric Equality

- **Only works with numbers**
- Tests mathematical equality, ignoring exact vs. inexact distinctions
- `(= 3 3.0)` → `#t`
- `(= 3 "3")` → **error** (not a number)

## `eq?` - Object Identity  

- **Strictest comparison** - tests if two values are the same object in memory
- Guaranteed to work for symbols, booleans, and `()`
- Behavior with numbers and characters is **implementation-dependent**
- `(eq? 'foo 'foo)` → `#t`
- `(eq? (cons 1 2) (cons 1 2))` → `#f` (different objects)

## `eqv?` - Operational Equivalence

- Like `eq?` but with **guaranteed behavior for numbers and characters**
- Same numbers with same exactness are `eqv?`
- Same characters are `eqv?`
- `(eqv? 3 3)` → `#t`
- `(eqv? 3 3.0)` → `#f` (different exactness)
- `(eqv? #\a #\a)` → `#t`

## `equal?` - Structural Equality

- **Most liberal** - recursively compares structure and content
- Two objects are `equal?` if they have the same structure with `eqv?` leaves
- `(equal? (list 1 2) (list 1 2))` → `#t`
- `(equal? "hello" "hello")` → `#t`
- `(equal? '(1 (2 3)) '(1 (2 3)))` → `#t`

## Rule of Thumb

- Use `=` for numeric comparisons
- Use `eq?` when you need object identity (rare)
- Use `eqv?` as the general-purpose equality test
- Use `equal?` when comparing compound data structures

Robert-Fisher_tmemu: What are the equality predicates in Common Lisp?

GitHub Copilot: In Common Lisp, there are four main equality predicates, each with different levels of strictness:

## `eq` - Object Identity

- **Strictest comparison** - tests if two objects are the same identical object
- Guaranteed to work reliably for symbols and keywords
- Behavior with numbers and characters is **implementation-dependent**
- Fast but limited in scope
- `(eq 'foo 'foo)` → `T`
- `(eq (cons 1 2) (cons 1 2))` → `NIL` (different objects)

## `eql` - Same Object or Equivalent Value

- Like `eq` but with **guaranteed behavior for numbers and characters**
- Numbers are `eql` if they're the same type and value
- Characters are `eql` if they're the same character
- **Default equality predicate** for many Common Lisp functions
- `(eql 3 3)` → `T`
- `(eql 3 3.0)` → `NIL` (different types)
- `(eql #\a #\a)` → `T`

## `equal` - Structural Equality

- **Recursively compares structure** for lists, strings, bit vectors, and pathnames
- Uses `eql` for atoms
- Most commonly used for comparing compound data structures
- `(equal (list 1 2) (list 1 2))` → `T`
- `(equal "hello" "hello")` → `T`
- `(equal '(1 (2 3)) '(1 (2 3)))` → `T`

## `equalp` - Liberal Structural Equality

- **Most liberal** comparison - ignores case in strings and characters
- Compares numbers by value regardless of type
- Recursively compares arrays, hash tables, structures, and instances
- `(equalp "Hello" "HELLO")` → `T`
- `(equalp 3 3.0)` → `T`
- `(equalp #(1 2) #(1 2))` → `T` (arrays)

## Numeric Equality: `=`

- **Only works with numbers**
- Compares numeric value, ignoring type differences
- Can compare multiple arguments
- `(= 3 3.0 3.0d0)` → `T`
- `(= 3 "3")` → **error** (not a number)

## Usage Guidelines

- Use `eql` as the default choice for most comparisons
- Use `eq` only when you specifically need object identity
- Use `equal` for comparing lists, strings, and tree structures  
- Use `equalp` when you need case-insensitive or type-flexible comparison
- Use `=` specifically for numeric comparisons

The hierarchy is: `eq` ⊆ `eql` ⊆ `equal` ⊆ `equalp` (each successive predicate considers more things equal).

Robert-Fisher_tmemu: What are the equality predicates in Kernel?

GitHub Copilot: In Kernel (the theoretical foundation for Noeval), equality is handled more minimally than in Scheme or Common Lisp, reflecting Kernel's philosophy of having fewer built-in predicates.

## `equal?` - The Primary Equality Predicate

- **Single equality test** - Kernel typically provides just one main equality predicate
- **Structural equality** - recursively compares compound data structures
- **Immutable-oriented** - designed for Kernel's immutable-by-default philosophy
- Works with all data types: numbers, symbols, pairs, strings

## `eq?` - Object Identity (if provided)

- **Implementation-dependent** - may or may not be provided
- Tests if two objects are literally the same object in memory
- Primarily useful for optimization, not general programming
- Behavior with numbers and other values is unspecified

## Key Differences from Scheme/Lisp

### Simplified Model

- **Fewer predicates** - avoids the complexity of multiple equality levels
- **Consistent behavior** - `equal?` works predictably across all data types
- **No numeric-specific equality** - numbers are compared through `equal?`

### Fexpr Integration

- Equality predicates are **operatives** that receive unevaluated arguments
- They evaluate their arguments to obtain values for comparison
- Fit naturally into Kernel's operative-based architecture

### Example Usage (Noeval-style)

````scheme
(equal? 42 42)                    ; #t
(equal? "hello" "hello")          ; #t  
(equal? (list 1 2) (list 1 2))    ; #t
(equal? 'symbol 'symbol)          ; #t
````

## Noeval Implementation Notes

In the Noeval interpreter, equality predicates would be implemented as built-in operatives that:

1. Evaluate their arguments to get the actual values
2. Perform deep structural comparison
3. Return Church Boolean functions for conditional usage

The minimalist approach aligns with Kernel's principle of providing essential primitives while keeping the core language simple and composable.

Robert-Fisher_tmemu: What are the equality predicates in Wat?

GitHub Copilot: Looking at the Wat virtual machine code, I can see that Wat doesn't define explicit equality predicates like Scheme's `eq?`, `eqv?`, and `equal?`. Instead, it relies on **JavaScript's native equality operators** through its JSNI (JavaScript Native Interface) system.

## Available Equality Operations in Wat

### JavaScript Operators via JSNI

Wat provides access to JavaScript's equality operators through its `define-js-binop` macro:

````javascript
["define-js-binop", "=="],    // JavaScript loose equality
["define-js-binop", "==="],   // JavaScript strict equality  
["define-js-binop", "!="],    // JavaScript loose inequality
["define-js-binop", "!=="],   // JavaScript strict inequality
````

These are used as:

- `(== a b)` - JavaScript loose equality (type coercion)
- `(=== a b)` - JavaScript strict equality (no type coercion)
- `(!= a b)` - JavaScript loose inequality
- `(!== a b)` - JavaScript strict inequality

## Semantic Differences from Lisp Equality

### `===` (Strict Equality)

- **Most similar to `eqv?`** - same type and value required
- `(=== 3 3)` → `true`
- `(=== 3 "3")` → `false` (different types)
- `(=== (cons 1 2) (cons 1 2))` → `false` (different objects)

### `==` (Loose Equality)

- **Type coercion applied** - more permissive than any Lisp equality
- `(== 3 "3")` → `true` (string coerced to number)
- `(== null undefined)` → `true` (JavaScript quirk)
- Generally **not recommended** due to unpredictable coercion rules

## Missing: Structural Equality

Unlike Scheme/Lisp systems, **Wat lacks a built-in structural equality predicate** equivalent to `equal?`. You would need to implement recursive structural comparison yourself using Wat's fexpr system.

## Wat's Design Philosophy

Wat's approach reflects its goal of being a **minimal fexpr layer over JavaScript** rather than a full Lisp system. It provides:

- Direct access to JavaScript's semantics
- Minimal abstraction overhead
- Fexpr-based metaprogramming capabilities

But it doesn't attempt to replicate traditional Lisp equality hierarchies, instead deferring to the host language (JavaScript) for basic operations.

Robert-Fisher_tmemu: In Noeval, I was inclined to make `=` like Scheme's `equal?`. It appears Kernel went that route too. Why might I end up regretting that choice...if I do?

GitHub Copilot: You might regret making `=` like `equal?` in Noeval for several reasons:

## Performance Implications

**Deep structural comparison is expensive**. Every use of `=` becomes a potentially recursive tree walk:

````scheme
(= huge-list-1 huge-list-2)  ; O(n) operation every time
````

In languages with multiple equality predicates, you can choose the right tool:

- Fast identity check when you need it
- Structural comparison only when necessary

With a single `=`, you lose that optimization choice.

## Semantic Ambiguity

**Different use cases need different equality semantics**:

````scheme
;; Do you want identity or structural equality here?
(define (memoize f)
  (let ((cache '()))
    (lambda (x)
      (let ((cached (assoc x cache)))  ; What = does assoc use?
        (if cached 
            (cdr cached)
            (let ((result (f x)))
              (set! cache (cons (cons x result) cache))
              result))))))
````

Hash tables, memoization, and set implementations often need identity-based equality for correctness, but structural equality for user convenience.

## Circular Structure Problems

**Structural equality can infinite loop**:

````scheme
(define x (cons 1 2))
(set-cdr! x x)  ; Create cycle
(= x x)         ; Might hang forever!
````

You'll need cycle detection, adding complexity and overhead to every equality check.

## Type System Tensions

**Numbers vs. everything else**:

````scheme
(= 3 3.0)     ; Should this be true?
(= 3 "3")     ; Definitely false
(= #t 1)      ; What about this?
````

Different types need different comparison semantics, but a single `=` forces one-size-fits-all decisions.

## Alternative Approaches

**Consider Kernel's actual approach more carefully** - it provides minimal primitives and lets the user build what they need:

````scheme
;; Maybe provide both, but make the choice explicit
(define eq? ...)      ; Fast identity
(define equal? ...)   ; Structural comparison
(define = equal?)     ; Alias for convenience
````

Or follow the **Clojure model** - use `=` for structural equality but provide `identical?` for identity when you need speed.

## The Real Issue

The core problem isn't the choice itself, but **losing the ability to choose**. Different algorithms need different equality semantics, and a single primitive forces you into one box.

You might not regret it if:

- You implement cycle detection properly
- Performance isn't critical for your use cases
- You provide an escape hatch (`eq?` or similar) for identity checks

But you **will** regret it if you later need high-performance code that does a lot of equality checking, or if you want to implement data structures that rely on identity-based equality for correctness.

Robert-Fisher_tmemu: I feel like all of these are special cases, and I can add support for them as the need arises. Better, I think, to keep `=` the common case and easy to understand and have the special cases—like Clojure's `identical?`—stand out and be clearly a special case.

GitHub Copilot: You're absolutely right, and that's a very sound design philosophy. Making `=` the **intuitive default** and having special cases be explicitly special is much better UX than forcing users to remember which of 4 different equality predicates to use.

## Why This Approach Works Well

### Principle of Least Surprise

````scheme
(= (list 1 2 3) (list 1 2 3))  ; Obviously should be true
(= "hello" "hello")            ; Obviously should be true
(= 42 42)                      ; Obviously should be true
````

Users expect `=` to mean "are these the same thing?" in the intuitive sense. Structural equality is what people mean 90% of the time.

### Special Cases Are Actually Special

When you need identity-based equality, you're usually doing something that **should** stand out:

````scheme
;; This looks special and performance-critical (good!)
(define (memoize f)
  (let ((cache '()))
    (lambda (x)
      (let ((cached (assoc/identical? x cache)))  ; clearly optimized
        ...))))

;; Or with a clear name
(identical? old-list new-list)  ; clearly checking object identity
````

### Clojure Got This Right

Clojure's design validates your approach:

- `=` is structural equality (the common case)
- `identical?` is identity (the optimization case)
- `==` is numeric equality (the type-flexible case)

Most Clojure code uses `=`, and when you see `identical?`, you know the author was thinking about performance.

## Implementation Strategy

You can start simple and add sophistication incrementally:

````cpp
// Phase 1: Simple structural equality
bool equals(const value& a, const value& b) {
    // Handle primitives, recursively compare structures
    // Don't worry about cycles initially
}

// Phase 2: Add cycle detection when needed
// Phase 3: Add identical? when performance matters
// Phase 4: Add numeric-specific equality if needed
````

## The Real Win

This approach means your core language stays **conceptually simple**. Users learn one equality operator that works intuitively. The complexity only appears when you actually need the edge cases, and then it's clearly marked as "I'm doing something special here."

Much better than Scheme's situation where beginners have to memorize the differences between `eq?`, `eqv?`, `equal?`, and `=` before they can even compare two lists.

Your instinct is spot on - optimize for the common case, make special cases explicit.

Robert-Fisher_tmemu: I'm also inclined to make `=` always raise an error if the arguments are of different type. Even in the case of floating point and rational numbers, the programmer should be alerted to such a comparison so they can make their intentions explicit.

GitHub Copilot: That's an excellent design decision that aligns perfectly with Noeval's philosophy of being explicit and intentional. Making `=` strict about types forces programmers to be clear about their intentions, which prevents subtle bugs.

## Why This Is a Good Choice

### Prevents Common Bugs

````scheme
;; These should be errors, forcing explicit choices
(= 3 3.0)        ; Error: int vs float
(= 3 3/1)        ; Error: int vs rational  
(= 1.0 1/1)      ; Error: float vs rational

;; Force explicit conversion or intent
(= 3 (exact 3.0))     ; Convert to exact
(= (inexact 3) 3.0)   ; Convert to inexact
(numeric-equal? 3 3.0) ; Explicit numeric comparison
````

### Follows Language Design Philosophy

- **Explicit over implicit** - no surprising type coercions
- **Fail fast** - catch type mismatches immediately  
- **Clear intent** - when you see `=` you know the types match
- **Minimal surprises** - structural equality with no hidden conversions

### Error Message Design

You can provide helpful error messages that guide users toward solutions:

````cpp
// In your equality implementation
if (a.type() != b.type()) {
    throw evaluation_error(
        expr_context(),
        "Type mismatch in equality: " + type_name(a) + " vs " + type_name(b) + 
        ". Use explicit conversion or numeric-equal? for cross-type numeric comparison"
    );
}
````

## Implementation Strategy (redux)

### Start Simple

````cpp
bool equal(const value& a, const value& b) {
    // Strict type checking first
    if (a.type() != b.type()) {
        return false; // or throw error depending on context
    }
    
    // Then structural comparison
    return structural_equal(a, b);
}
````

### Add Numeric Utilities Later

When you need cross-type numeric comparison:

````scheme
;; Built-in operative for when you actually want numeric equality
(define numeric-equal? 
  (operative (env a b) 
    (and (number? a) (number? b) (numeric-compare a b))))

;; Conversion utilities
(define (exact x) ...)     ; rational/int from float
(define (inexact x) ...)   ; float from rational/int
````

## Benefits Over Permissive Equality

### Debugging Advantage

````scheme
;; Bug reveals itself immediately
(= user-input expected-value)  ; Error if types don't match

;; vs hidden bug with coercion
(= "42" 42)  ; Would this be true? False? Depends on coercion rules...
````

### Performance Advantage

- **Fast type check first** - no expensive conversion attempts
- **No coercion overhead** - direct structural comparison
- **Predictable performance** - no surprise conversions

### Mathematical Clarity

Different numeric types have different semantics:

- **Integers**: exact, unlimited precision
- **Rationals**: exact fractions  
- **Floats**: approximate, limited precision

Conflating them in `=` hides important distinctions that affect program correctness.

This approach makes Noeval's `=` both **fast and safe** - exactly what you want in a foundational primitive. Users who need cross-type comparison will reach for the right tool explicitly, making their code more maintainable and their intentions clear.

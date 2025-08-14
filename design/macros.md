# Macros

Common Lisp:
  defmacro: procedural macros
  symbol-macro: expand a symbol e.g. (define-symbol-macro pi 3.1415926)
  reader macros
  define-compiler-macro ?
SLIB: defmacro
  required a modified `load` procedure or other way to do the macro expansion
Syntactic closures: procedural with hygiene
R4RS: syntax-rules
SRFI-72 (after R5RS): procedural with hygiene
R6RS: syntax-case (in addition to syntax-rules)
  Procedural
  Control over when a macro versus its expanded code runs
  for-syntax, for-template, and phase levels
  Syntax objects
R7RS Small: syntax-rules and syntax-case but no phase levels
SRFI-148: eager-syntax-rules
  Allows computation at definition time within syntax-rules
Racket: syntax-parse along with enhanced syntax-case

```scheme
; SLIB defmacro
(defmacro when (test . body)
  `(if ,test (begin ,@body)))

; Syntactic closure
(define-syntax when
  (syntax-closure-transformer
  ; sc-macro-transformer in MIT Scheme
    (lambda (form env)
      (let ((test (cadr form))
            (body (cddr form)))
        `(if ,test (begin ,@body))))))

; Chicken Scheme Reverse syntactic closure
(define-syntax when
  (rsc-macro-transformer
    (lambda (form rename compare)
      (let ((test (cadr form))
            (body (cddr form)))
        `(,(rename 'if) ,test (,(rename 'begin) ,@body))))))

; Explicit renaming syntactic closure
(define-syntax when
  (er-macro-transformer
    (lambda (form rename compare)
      (let ((test (cadr form))
            (body (cddr form)))
        `(,(rename 'if) ,test (,(rename 'begin) ,@body))))))

; R4RS sytax-rules
(define-syntax when
  (syntax-rules ()
    ((when test body ...)
     (if test (begin body ...)))))

; SRFI-72 macro
(define-syntax (when test . body)
  (quasisyntax (if ,test (begin ,@body) nil)))

; R6RS syntax-case
(define-syntax when
  (lambda (stx)
    (syntax-case stx ()
      ((when tset body ...)
       #'(if test (begin body ...))))))

; SRFI-148
(define-syntax when-debug
  (eager-syntax-rules ()
    ((when-debug level test body ...)
     (if (and (>= current-debug-level level) test)
         (begin body ...)))))

; Racket syntax-case with source location preservation
(define-syntax when
  (lambda (stx)
    (syntax-case stx ()
      [(when test body ...)
       (with-syntax ([if-form (datum->syntax stx 'if)])
         #'(if-form test (begin body ...)))])))

; syntax-parse
(define-syntax when
  (syntax-parser
    [(_ test:expr body:expr ...+)
     #'(if test (begin body ...))]))
```

## define-syntax

The `define-syntax` form binds a macro transformer to a name and marks it as
a macro instead of a regular variable.

Most Scheme implementations use the same namespace for macros and variables.

A macro transformer is, ultimately, a lambda that takes a single parameter
representing the syntax to be transformed.

```scheme
; defmacro style with define-syntax
; Doesn't work because syntax objects aren't plain objects
; And we need to return a syntax object
(define-syntax when
  (lambda (stx)
    (let ((test (cadr stx))
          (body (cddr stx)))
      `(if ,test (begin ,@body)))))

; Works in Racket
(define-syntax test
  (lambda (stx)
    #`(begin
        (write #,@(cdr (syntax->datum stx)))
        (newline))))

(test '(this is a test))

; Create defmacro style transformers
; The `begin-for-syntax` is needed to make the helper available at macro
; expansion time.
; (Tested in Racket)
(begin-for-syntax
  (define make-defmacro-transformer
    (lambda (transformer)
      (lambda (stx)
        ; Pass the original stx to datum->syntax to preserve the original
        ; lexical context. "Make this new syntax object behave as if it
        ; appeared in the same lexical context as the original macro call."
        (datum->syntax stx (transformer (syntax->datum stx)))))))

(define-syntax test
  (make-defmacro-transformer
    (lambda (original)
      `(begin
        (write ,@(cdr original))
        (newline)))))

(test '(this is a test))

;; R6RS way
(library (defmacro-helper)
  (export make-defmacro-transformer)
  (import (rnrs))
  
  (define make-defmacro-transformer
    (lambda (transformer)
      (lambda (stx)
        (datum->syntax stx (transformer (syntax->datum stx)))))))

(library (test-macro)
  (export test)
  ; This import specifies that defmacro-helper can be used by macros
  (import (rnrs) (for (defmacro-helper) expand))
  
  (define-syntax test
    (make-defmacro-transformer ...)))
```

SLIB `defmacro` could directly use any procedure defined before the macro. It
did not have the same phasing issues as R6RS macros do.

The same is true for syntactic closures.

`Syntax-rules` doesn't have such phasing issues since it doesn't call
procedures at all.

SRFI-148 introduces procedure calls into `syntax-rules`, but they are resolved
at *definition* time. (So...different phasing issues.)

SRFI-72 macros can often avoid the phasing issues by reducing the need for
helper procedures. It makes no provision for addressing it.

MIT: `(load-option 'synchronous-subprocess)`
Chicken: Modules can use `import-for-syntax`
Guile: `eval-when`
Chez: `(meta define helper (lambda ...))`
Gambit: `(##include "helpers.scm")`

The `eval-when` has the advantage of being able to specify that the procedure
is available in multiple phases. R6RS can do that too by importing the module
multiple times.

Guile borrowed `eval-when` from Common Lisp where it provides three phases:
`:compile-toplevel :load-toplevel :execute` (The "-toplevel" suffix doesn't
really mean much since all the phases are essentially "top level".)

Macro expansion in CL happens during the compile phase.

Racket just has expansion time and runtime.

Wat uses a single phase.

## Common Lisp compile time

CL's `eval-when` can perform any computation at compile time. Compile time
computation doesn't need to be rooted in a macro call. A full program could be
reduced to a single compile-time phase.

Even user interaction, file I/O, and network I/O can be done at compile time.

Some things can't be done at compile time, though:

* Reading command line arguments
* Accessing runtime environment state (e.g. environment variables)
* Some system calls (for security)
* Threading
* Some FFI calls

I've not yet seen what problem multiple phases solves. (Which is not to say
that it doesn't solve problems; jusit that I don't see it yet.)

## The implementation of Wat

How does parsing and evaluation proceed in Wat? How does macro expansion fit
into it?

```javascript
Cons.prototype.wat_eval = function(e, k, f) {
    // 1. Evaluate operator (first element)
    var op = evaluate(e, null, null, car(this));
    
    // 2. Handle macros vs normal combiners
    if (isMacro(op)) {
        return macroCombine(e, null, null, op, this);
    } else {
        return combine(e, null, null, op, cdr(this));
    }
};
```

(Note that `evaluate` calls `wat_eval`.)

The three phases—parsing, evaluating, executing—are done on a single expression
at a time.

```javascript
function run(x) { 
    return evaluate(environment, null, null, parse_json_value(x)); 
}
```

Macros, which happen in `evaluate`, cause the expression to be reëvaluated.

```javascript
function macroCombine(e, k, f, macro, form) {
    var expanded = combine(e, k, f, macro.expander, cdr(form));
    // MUTATE the original form
    form.car = expanded.car;
    form.cdr = expanded.cdr;
    return evaluate(e, k, f, form); // Re-evaluate transformed AST
}
```

Looking at the `evaluate` function and the broader codebase, there's an important distinction between **evaluation** and **execution** in Wat:

### Evaluation vs Execution

#### Evaluation (`evaluate`)

**Evaluation** is the process of determining what an expression *means* - converting syntax to semantic values:

````javascript
function evaluate(e, k, f, x) {
    if (x && x.wat_eval) return x.wat_eval(e, k, f); else return x;
}
````

Evaluation handles:

* **Symbol lookup**: `Sym.prototype.wat_eval` looks up bindings
* **Form recognition**: `Cons.prototype.wat_eval` identifies function calls
* **Macro expansion**: Converting macro forms to expanded forms
* **Value resolution**: Turning expressions into values

#### Execution (`wat_combine`)

**Execution** is the process of *performing actions* with those values - actually carrying out computations:

````javascript
function combine(e, k, f, cmb, o) {
    if (cmb && cmb.wat_combine)
        return cmb.wat_combine(e, k, f, o);
    // ...
}
````

Execution handles:

* **Function application**: Running operative/applicative combiners
* **Side effects**: Binding variables, I/O, mutations
* **Control flow**: Conditionals, loops, exceptions
* **State changes**: Environment modifications

### The Flow

Here's how they work together:

````javascript
// 1. EVALUATION: Parse and resolve what this means
Cons.prototype.wat_eval = function(e, k, f) {
    var op = evaluate(e, null, null, car(this)); // Evaluate operator
    // ...
    return combine(e, null, null, op, cdr(this)); // Trigger execution
};

// 2. EXECUTION: Actually perform the operation
Opv.prototype.wat_combine = function(e, k, f, o) {
    var xe = new Env(this.e); 
    bind(xe, this.p, o);           // EXECUTE: Bind parameters
    bind(xe, this.ep, e);          // EXECUTE: Bind environment
    return evaluate(xe, k, f, this.x); // Back to EVALUATION
};
````

### Key Differences

| Aspect | Evaluation | Execution |
|--------|------------|-----------|
| **Purpose** | Determine meaning | Perform actions |
| **Entry Point** | `evaluate()` | `wat_combine()` |
| **Handles** | Syntax → Semantics | Semantics → Effects |
| **Examples** | Symbol lookup, macro expansion | Function calls, variable binding |

### Concrete Example

````javascript
// Expression: ["def", "x", ["+", "1", "2"]]

// EVALUATION phase:
// 1. evaluate(env, null, null, ["def", "x", ["+", "1", "2"]])
// 2. Cons.wat_eval recognizes this as a function call
// 3. evaluate(env, null, null, "def") → returns Def combiner
// 4. combine(env, null, null, Def, ["x", ["+", "1", "2"]])

// EXECUTION phase:  
// 5. Def.wat_combine executes the definition:
//    - evaluate(env, null, null, ["+", "1", "2"]) → 3 (more evaluation!)
//    - bind(env, "x", 3) → side effect (execution!)
````

The phases are **interleaved** - execution often triggers more evaluation (like when `Def` evaluates the right-hand side), and evaluation eventually leads to execution through the `combine` mechanism.

This separation allows Wat to have powerful metaprogramming features where evaluation can be suspended, modified, or resumed through continuations and macros.

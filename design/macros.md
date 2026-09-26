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

## Design for Noeval

This section is the design for expansion-time macros in Noeval, following the
idea in [Having both fexprs and macros](https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html).
Nothing here is implemented yet.

### Motivation

The previous `cond` (kept in a `#skip` block in `src/lib.noeval`) transformed
its clauses into an `if` chain on every call, which made it about 14 times
slower than the `if` chain alone. The current `cond` evaluates its clauses
directly and is about 1.7 times slower. A macro can do the transformation
once, and can check every clause up front. `let`, `when`, and `unless` also
rebuild code on every call, and `let` validates its bindings on every call.

### Constraints from the language

- An operative receives its operands as data, and may never evaluate them. So
  an expander can't know which subforms are code: `(q (when a b))` must not be
  expanded.
- `eval_operation` already accepts an operative value in operator position, so
  code can contain values instead of symbols. `when` and `unless` already
  build their code this way.
- Noeval has no `set-car!`, so the operands of a combination can't be changed
  from Noeval code.
- `cons_cell` already has a field that isn't part of its value: `location`.
- An operative sees only its operands, never the combination it was called
  from, so a library implementation has nowhere to keep a per-call-site cache.

### The `macro` primitive

`(macro operative)` returns a macro, a new kind of value (`typeof` gives
`macro`). When a combination's operator evaluates to a macro, the evaluator:

1. calls the transformer operative with the combination's unevaluated
   operands,
2. evaluates the result (the expansion) in the calling environment, as a tail
   call, and
3. caches the expansion on the combination (see below).

`wrap` and `macro` both take an operative. `wrap` changes what happens to the
operands (they are evaluated first); `macro` changes what happens to the
result (it is evaluated in the calling environment). Without the cache, a
macro is equivalent to this library operative, which is the pattern the old
`cond` used:

```scheme
(vau operands env (eval (call-transformer operands) env))
```

The cache is the reason for the primitive; the library can't provide it.

The transformer receives the operands unevaluated, so it should be written with
`vau`, not `lambda`. (`macro` can't tell them apart, since `wrap` is a library
operative and there's no separate applicative type.)

The transformer is called with a fresh, empty environment as its environment
argument. An expansion therefore depends only on the operands, which is what
makes caching sound, and a transformer can't have side effects on the calling
environment that would happen only on the first call. A transformer that
tries to use its environment fails loudly.

```scheme
(define when
  (macro (vau args _ (list if (first args) (cons do (rest args)) ()))))
```

### When expansion happens

Expansion is lazy: a combination is expanded the first time it's evaluated,
using the environment it's evaluated in and the value its operator actually
has. The expansion is cached, so later evaluations of the same combination
skip the transformer.

The alternatives were rejected:

- **When an operative is created** (walking the `vau` body before storing it):
  it would expand data as well as code, since it can't tell which operands
  will be evaluated. It would resolve names before the body's environment
  exists, missing parameters and local definitions that shadow a macro. It
  would miss macros defined after the operative (the old `cond-transformer`
  uses `and`, which is defined after `cond`). It wouldn't cover top-level
  forms or `eval`, so lazy expansion would be needed anyway. And it would walk
  bodies that are never called.
- **At load time** (expanding each top-level form before evaluating it): all
  of the above, plus `(do (define m (macro ...)) (m ...))` wouldn't work, and
  it brings back the phase problems described earlier in this document.

Lazy expansion has a single phase. A transformer is an ordinary operative
that closes over its environment, so it can use any helper defined before it
runs.

The cost is that a macro's checks run the first time the combination is
evaluated, not when the enclosing operative is created. A `cond` in an
operative that's never called is never checked.

#### The cache

Wat memoizes by replacing the combination's cells in place. In Noeval that
would be visible, since code is data: an operative that evaluates its operand
and then returns it would return the expansion. Instead, the expansion is
kept in a hidden field of the combination's `cons_cell`, like `location`.
`first`, `rest`, printing, and `=` don't see it. (`eval_operation` currently
copies the cell with `value::make(cell)`, so it will need the original
`value_ptr` to write the cache.)

The cache holds the macro value it was expanded with, as well as the
expansion. Operatives are first-class, so the same combination can be
evaluated with different values for its operator: the same `vau` body closed
over different environments, a parameter bound to different macros,
`redefine` at the REPL, or `eval` in a different environment. So each
evaluation still evaluates the operator (the lookup a call pays anyway). If
the result is the same macro object as the cached one, the cached expansion is
evaluated. Otherwise the combination is expanded again if the operator is a
macro, or called normally if it isn't. Since operands can't be mutated and
expansions depend only on the operands, the same combination and the same
macro always give the same expansion.

Consequences:

- A combination built at runtime, such as `(eval (cons m args) env)`, is
  expanded every time it's evaluated. That's correct, just not faster.
- Combinations inside an expansion are cached on the expansion's cells, which
  the cache keeps, so macros that expand into other macro calls work.
- Every `cons_cell` grows by a pointer, including those in list data. The
  benchmarks' peak memory will show the cost. (A side table keyed by cell
  address is the alternative; it may be revisited.)
- The cycle collector must scan the cached macro and expansion. Missing them
  would cause leaks, not corruption, but they have to be counted.
- Expansions have no source location, so errors inside one are reported at the
  macro call. Copying the call's location to the top cell of the expansion
  would help.

### Hygiene

There are no renaming or syntax objects. Capture is avoided by convention:

- **Names inserted by a macro captured by user bindings** (a local `do` or `if`
  breaking `when`): a macro inserts values, not symbols, as Wat does and as
  `when` and `unless` already do. The old `cond` inserts `(q do)`, which will
  become the `do` value. An operative or environment value can only be
  evaluated in operator position; elsewhere it has to be quoted with
  `(list q value)`. Making every value other than a symbol or a cons cell
  evaluate to itself, as Kernel does, would remove that restriction, but it
  isn't needed yet.
- **User names captured by bindings a macro introduces**: an expansion doesn't
  introduce bindings. Temporaries belong in an embedded operative, which
  receives the user's code as operands and evaluates it in the calling
  environment, so its own names are in its own environment. `cond`, `when`,
  and `unless` introduce no names, and `let` introduces only the user's.
  There's no `gensym` (or `string->symbol`); one can be added if a macro needs
  it.

A drawback is that expansions print as `(#<operative...> ...)`, which is
harder to read when debugging.

### Interaction with the rest of the language

- **`do` and `try` don't create environments.** An expansion is evaluated in
  the calling environment, so `(when c (define x 1))` still defines `x` there.
  A macro shouldn't wrap its expansion in a `lambda`.
- **`define` can't rebind a name.** Expansion itself never defines anything,
  since the transformer gets an empty environment. The expansion is evaluated
  every time, exactly as if the user had written it. An expansion that defines
  the same name twice fails with `define`'s usual error, as does one evaluated
  twice in the same environment. A macro that duplicates an operand
  (`(twice e)` expanding to `(do e e)`) fails for `(twice (define y 1))`; that's
  the macro author's bug, as in any Lisp. Only the transformation is cached,
  not the evaluation, so caching changes none of this.
- **`redefine`**: the macro identity check makes cached expansions of a
  redefined macro stale, so they're expanded again.
- **First-class environments**: expansion adds no names to any environment.
  `macro` is a new name in the builtins environment, and macros can be
  recognized with `typeof`. The cache is independent of the environment, so
  the same code evaluated with `eval` in two environments shares an
  expansion. That's correct as long as expansions embed values rather than
  depending on how the user's symbols are bound.
- **Minimize primitives**: the additions are one primitive, one value type,
  and a hidden field.

### Uses

- **`cond`**: bring back `cond-transformer`, with `(q do)` replaced by `do`,
  and define `(define cond (macro (vau clauses _ (cond-transformer clauses))))`.
  This removes `cond-clauses`, `cond-clause`, `cond-test`, and `cond-body`,
  leaving `cond-transformer` as the only helper. `cond` will report a
  malformed clause the first time it's evaluated, even one after the clause
  chosen, so the test "cond should not check clauses after the one chosen" in
  `tests/control-flow.noeval` will be replaced, and the comment above `cond`
  updated.
- **`when` and `unless`**: one-line conversions.
- **`let`**: validate the bindings once and expand to
  `((lambda* names body ...) values ...)`. The skipped test "let should reject
  malformed bindings" in `tests/environments.noeval` should be enabled.
- **`if`**: `(if c a b)` could expand to `(c a b)`, removing an `eval` from
  every `if`.

### Testing

C++ tests (`src/tests.cpp`):

- The hidden cache is ignored by `=` and by printing.
- A cache hit, and a cache miss when the operator's value changes.
- The cycle collector counts the cache's references (also run under GC
  stress).

Library tests (a new `tests/macros.noeval`):

- The transformer runs once per combination: a `define-mutable` counter
  incremented by the transformer is 1 after calling an operative containing
  the macro call three times, and 2 with two call sites.
- A combination whose operator is a parameter gives the right result when the
  parameter is bound to one macro, then another, then an ordinary operative.
- Quoted forms aren't expanded, and `(vau (x) env (do (eval x env) x))` returns
  the form it was given, not the expansion.
- Hygiene: `(let ((do 5) (if 6)) (when true 1))` works.
- A `define` inside `when` or `cond` binds in the calling environment, and
  `(twice (define y 1))` raises `define`'s error.
- Errors and their messages: `(macro 5)`, an error raised by a transformer, a
  transformer that uses its environment, and a malformed `cond` (reported
  with the call's location).
- The macro gives the same results as the uncached library operative above.

The existing `cond`, `when`, `unless`, and `let` tests should pass unchanged,
apart from the `cond` test noted above.

### Benchmarking

Save results before the change, then compare with `benchmarks/run.bash -c`:

- `cond`, `if-chain`, and `codepoints-utf8`: `cond` should approach
  `if-chain`, with the remaining difference being the operator lookup and the
  cache check.
- `startup` and `library-tests`: the time and peak memory cost of the larger
  `cons_cell`, which every benchmark pays.
- Possibly a new benchmark that uses `let`, `when`, and `unless` heavily.

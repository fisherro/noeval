# TODO

## Ongoing

What primitives can we get rid of?

Should any library functions take advantage of other library functions or
primitives that they aren't?

Review code for conformance to the style guidelines

## Regular

map or foldl and other HOF

append, length, reverse

filter

During `define`, attach the name to the value directly for debugging?
Values can have a flat_set of names?
Include a (weak) reference to the environment?

TCO

First-class delimited continuations

Add expansion-time macros (see https://axisofeval.blogspot.com/2012/09/having-both-fexprs-and-macros.html )

* A `macro` primitive works kind of like `wrap` to turn any operative into
* a macro transformer.
* Need a macro transformer primitive that will expand macros.
* When the code is creating an operative, run macro expansion on the body before storing it in the operative.

There's a lot of common code that could be refactored in the builtin operatives

Pattern matching?

Need to be able to catch errors in tests. Ideally that happens after we add
continuations, but it may be important enough to do something temporary.

"Numeric tower"...or at least support of bignums

* All numbers are boost::cpp_rational
* Default formatting is (possibly) repeating decimals
* Floats will be boost::cpp_dec_float

Use demangle in value_to_string?

Revisit the uses of std::visit

Look for places we could simplify code with ranges

Revisit escaping in string literals (what scheme do we want to use?)

Replace lists with arrays?

> I'll avoid set-car!/set-cdr! for now...a set-array-element! might happen
> And that might be needed for a good promise implementation

Consider adding an equivalent to Kernel's #ignore

Module system?

Hash sets and maps

Interning symbols

Support for lazy evaluation (We can do this in the library, right?)

FFI and POSIX support

Create let

* Advantages of let over define...
* Bindings only happen at the top of the let and cannot be rebound within the body
  * We'd need to ensure that define can't rebind something bound with let
* More functional...define does a mutation

Look for places we can use string_view

Consider whether to adopt Kernel's $ naming convention

Would it make sense to implement `if` in terms of `cond`?

Implement void or #inert?

Dynamic variables (a la Kernel?) (could be used for test-failures in the library tests)

Consolidate write and display into a single primitive

Consider if*, cond*, and*, or*, not* library functions that can handle
non-boolean values

Rewrite any uses of if into Church Boolean form in the library

Do we need get-current-environment? Is it correct?

Simplify `if`

> `(if a b c)` is the same as `(a b c)`

`for-each*` that can handle *n*-ary procedures?

Change equal_operative to use op== and/or use other type-specific customization point

Move the testing infrastructure into the library?
Or wait until we have modules and put it in a module?

## Exceptions (w/o continuations) notes

Collecting workspace informationLooking at your Noeval interpreter and its philosophy, let me address your questions about error handling:

## Would C++ exception catching fit Noeval's philosophy?

**Not really.** Noeval emphasizes:
- First-class operatives and fexprs
- Everything expressible in the language itself
- Minimal primitives with maximum power

Having error handling rely on C++ exceptions would break this philosophy by making error handling a special case that can't be implemented or controlled from within the language.

## Continuations: Big lift or manageable?

Continuations are definitely a significant undertaking, but given your current architecture, they might be more manageable than you think:

**Current advantages:**
- You already have `environment` chains for lexical scoping
- Your `eval` function has a clean recursive structure
- You have `call_stack` tracking for debugging

**What you'd need:**
- Capture the current continuation (call stack + environment chain)
- A way to invoke continuations (jump back to a captured point)
- Integration with your existing `operative` system

## Noeval-philosophy error handling without continuations

You could implement a simpler error handling system that still fits the philosophy:

```noeval
;; Error handling operative that captures errors
(define try-catch
  (vau (try-expr catch-expr) env
    ;; Implementation would catch C++ exceptions internally
    ;; but expose them as first-class values
    ))

;; Usage:
(try-catch 
  (/ 1 0)                    ; might throw
  (lambda (error) "caught")) ; error handler
```

This would:
- Use operatives (fits the philosophy)
- Make errors first-class values
- Allow error handling to be implemented in the library
- Still catch C++ exceptions internally, but wrap them properly

## My recommendation: Implement a simple solution now

Here's why I'd suggest a temporary solution:

1. **Immediate need**: You mention in TODO.md that you need error handling for tests
2. **Learning value**: You'll learn more about Noeval by having working error handling
3. **Incremental progress**: You can always replace it with continuation-based handling later
4. **Practical constraints**: Continuations are complex enough to potentially stall other progress

## Implementation approach

Add a `try_operative` to your builtins:

````cpp
// In builtins namespace
value_ptr try_operative(const std::vector<value_ptr>& args, env_ptr env)
{
    if (args.size() != 2) {
        throw evaluation_error("try: expected 2 arguments (expr handler)");
    }
    
    auto try_expr = args[0];
    auto handler_expr = args[1];
    
    try {
        return eval(try_expr, env);
    } catch (const evaluation_error& e) {
        // Create error value as a list: (error "message" "context")
        auto error_sym = std::make_shared<value>(symbol{"error"});
        auto msg_val = std::make_shared<value>(e.message);
        auto ctx_val = std::make_shared<value>(e.context);
        
        auto error_list = std::make_shared<value>(cons_cell{
            error_sym,
            std::make_shared<value>(cons_cell{
                msg_val,
                std::make_shared<value>(cons_cell{
                    ctx_val,
                    std::make_shared<value>(nullptr)
                })
            })
        });
        
        // Evaluate handler with error as argument
        auto handler_call = std::make_shared<value>(cons_cell{
            handler_expr,
            std::make_shared<value>(cons_cell{
                error_list,
                std::make_shared<value>(nullptr)
            })
        });
        
        return eval(handler_call, env);
    }
}
````

Then in lib.noeval:

```noeval
;; Library functions for error handling
(define error? 
  (lambda (obj) 
    (and (not (nil? obj)) 
         (= (first obj) (q error)))))

(define error-message 
  (lambda (error) 
    (first (rest error))))

;; Usage in tests:
(define safe-divide
  (lambda (a b)
    (try (/ a b)
         (lambda (err) 
           (if (error? err)
               "division error"
               err)))))
```

This approach:
- ✅ Fits Noeval's operative-based philosophy
- ✅ Makes errors first-class values
- ✅ Allows error handling in the library
- ✅ Provides immediate utility for testing
- ✅ Can be replaced later with continuation-based handling

You're not being too scared of continuations - they're genuinely complex. Getting experience with Noeval through a working error handling system will better prepare you for implementing continuations later.
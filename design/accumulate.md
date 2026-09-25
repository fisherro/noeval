# Accumulation

The library now provides `foldl-until`, `foldr-until`, `unfoldl`, and
`unfoldr`. See "What the library does" at the end.

## Scheme do

```scheme
(do ((var1 init1 step1)
     (var2 init2 step2)
     ...)
    (test result ...)
    body ...)
```

* Supports accumulators other than lists
* Supports multiple accumulators
* Supports early termination
* Arguably worse as syntax than as a HOF like an unfold

## SRFI-1 unfold

```scheme
(unfold p f g seed) =
    (if (p seed) (tail-gen seed)
        (cons (f seed)
              (unfold p f g (g seed))))
```

The `tail-gen` is an optional additonal parameter that defaults to
`(λ (seed) '())`.

```scheme
(let lp ((seed seed) (lis tail))
  (if (p seed) lis
      (lp (g seed)
          (cons (f seed) lis))))
```

The `tail` is an optional additional parameter that defaults to `'()`.

* Accumulates a single list from arbitrary value(s)
* Supports early termination

## SRFI-1 fold

* Accumulates an arbitrary value from a list
* Does *not* (typically) support early termination

## Comparison

SRFI-1:

> `unfold` is the fundamental recursive list constructor, just as `fold-right` is the fundamental recursive list consumer.

Also SRFI-1:

> `unfold-right` is the fundamental iterative list constructor, just as `fold` is the fundamental iterative list consumer.

### do-based map

```scheme
(define (map proc lyst)
  (do ((remaining lyst (rest remaining))
       (acc () (cons (proc (first remaining))
                     acc)))
    ((null? remaining)
     (reverse acc))))
```

Hypothetical `inverse-do`:

```scheme
(define (map (proc lyst)
  (inverse-do ((remaining lyst (rest remaining))
               (acc () (cons (proc (first remaining))
                                   acc)))
    ((null? remaining)
     remaining))))
```

### fold-based map

```scheme
(define (map proc lyst)
  (fold-right (lambda (element acc)
                (cons (proc element)
                      acc))
                ()
                lyst))
```

```scheme
(define (map proc lyst)
  (reverse (fold (lambda (acc element)
                   (cons (proc element)
                         acc))
                 ()
                 lyst))
```

### unfold-based map

```scheme
(define (map proc lyst)
  (unfold null?
          (λ (remaining)
             (proc (first remaining)))
          rest
          lyst))
```

```scheme
(define (map proc lyst)
  (reverse (unfold-right null?
                         (λ (remaining)
                            (proc (first remaining)))
                         rest)))
```

## Musings about the Noeval library

### folds and unfolds

Library functions that can be implemented as a fold:

* `length`
* `reverse`
* `append`
* `any?`
* `all?`
* `filter`
* `map`
* `last`
* `list-index`

Both `any?` and `all?`, however, need early termination, and folds do not
(typically) provide for that.

Library functions that can be implemented as an unfold:

* `iota`
* `take`
* `drop`

While `take` and `drop` benefit from early termination, unfolds do support that.

### Scheme like do loop

Library functions that might be implemented with a do loop:

* `list-index`
* `any?`
* `utf8->codepoints`
* `countdown`

...and inverse do loop...

* `take`

### fold with stop-pred

```noeval
(define foldl-until
  (lambda* (func init lyst stop-pred)
    (unless (operative? func)
      (raise "foldl-until's first argument must be callable"))
    (unless (list? lyst)
      (raise "foldl-until's third argument must be a list"))
    (unless (operative? stop-pred)
      (raise "foldl-until's fourth argument must be a predicate"))
    (define loop
      (lambda (acc remaining)
        ((or (nil? remaining) (stop-pred acc))
         acc
         (loop (func acc (first remaining))
               (rest remaining)))))
    (loop init lyst)))
```

### Scheme's do as a HOF

```noeval
; do-loop: Higher-order function version of Scheme's do construct
; Takes: variable-specs, test-func, step-func, body-func
; where variable-specs is a list of (name initial-value step-expr) triples
(define do-loop
  (lambda* (var-specs test-func step-func body-func)
    (unless (list? var-specs)
      (raise "do-loop's first argument must be a list of variable specifications"))
    (unless (operative? test-func)
      (raise "do-loop's second argument must be a test function"))
    (unless (operative? step-func)
      (raise "do-loop's third argument must be a step function"))
    (unless (operative? body-func)
      (raise "do-loop's fourth argument must be a body function"))
    
    ; Extract initial values
    (define init-values (map second var-specs))
    
    ; Create the loop function
    (define loop
      (lambda current-values
        (do
          ; Test termination condition
          (define test-result (apply test-func current-values))
          ((first test-result)  ; assuming test returns (continue? result)
           (second test-result)  ; return the result
           (do
             ; Execute body with current values
             (apply body-func current-values)
             ; Compute next values and continue
             (define next-values (apply step-func current-values))
             (apply loop next-values))))))
    
    ; Start the loop with initial values
    (apply loop init-values)))
```

Using a `vau` and could make the `var-specs` better while still eagerly
evaluating the other parameters.

Potentially provide both HOF and syntax transformation versions:

```noeval
; Syntax version for convenience (what we discussed earlier)
(define do-syntax
  (vau operands env
    ; ... syntax transformation implementation
    ))

; HOF version for functional composition
(define do-hof
  (lambda* (var-specs test-func step-func body-func)
    ; ... HOF implementation above
    ))

; Helper to convert between them
(define syntax->hof-do
  (vau operands env
    ; Convert syntax form to HOF calls
    ; This would transform the declarative syntax into function arguments
    ))
```

## What the library does

The library takes the fold/unfold route rather than a `do` loop.

* `(foldl-until func init lyst stop?)` is `foldl` that calls `(stop? acc)`
  before each step and returns `acc` once it's true.
* `(foldr-until func lyst init stop?)` is the mirror image. Working from the
  right, it returns `acc` once `(stop? acc)` is true, skipping the elements to
  the left. (It still walks the whole list to build its continuation chain.)
* `(unfoldl stop? mapper successor seed)` is the accumulator pattern. It's
  SRFI-1's `unfold-right`: the first element generated ends up last.
* `(unfoldr stop? mapper successor seed)` is the accumulate-then-reverse
  pattern. It's SRFI-1's `unfold` (and Haskell's `unfoldr`): the first element
  generated ends up first.

The `l` and `r` match the folds each one inverts, as SRFI-1 pairs `unfold`
with `fold-right` and `unfold-right` with `fold`.

Library functions built on them:

* `foldl`: `length`, `reverse`, `last`, `for-each`, `filter`, `append`
* `foldl-until`: `any?`, `all?`, `list-index`
* `unfoldl`: `iota`
* `unfoldr`: `map`, `take`, `utf8->codepoints`

`drop` and `nth` aren't accumulations. They walk down the list and return
what's left, so they keep their own loops.

# Dependency checker

This will be a test of writing a practical program in Noeval.

It will...

* Get a list of builtins
* Read a file of Noeval code (such as lib.noeval).
* When reading a top-level `define` noting...
  * Expression count
  * Name
  * Applications/invocations of non-local symbols
  * Non-local and non-self-referential symbols referenced by the definition
* Then report any definitions that precede their dependencies.

## Noeval enhancements needed

* A way to get a list of the builtins.
* A way to read expressions from a file.

### Reading

The typical way this would work might be:

```scheme
(define (read-all-expressions filename)
  (call-with-input-file filename
    (lambda (port)
      (let loop ((exprs '()))
        (let ((expr (read port)))
          (if (eof-object? expr)
              (reverse exprs)
              (loop (cons expr exprs))))))))
```

We can simplify what we need to implement by writing the checker to use stdin.
This way we only need to implement:

* `read`
* `eof-object`
* `eof-object?`

## Caveats

For the first version, we won't track scope to avoid false positives from shadowed names. For example:

```noeval
(define helper (lambda (x) x))  ; line 10

(define main                    ; line 20
  (lambda* (data)
    (define helper (lambda (y) (* y 2)))  ; local shadow
    (helper data)))             ; refers to local, not global
```

The analyzer might report that `main` depends on the global `helper` (line 10), even though it actually uses a local binding. In practice, this should be rare in well-structured code where local bindings use different names than global ones.

## Possible implementation structure

```noeval
(define read-all-expressions
  (lambda* ()
    (let loop ((exprs '()) (position 0))
      (define expr (read))
      (cond ((eof-object? expr)
             (reverse exprs))
            (else
             (loop (cons (list expr position) exprs)
                   (+ position 1)))))))

(define extract-definition-info
  (lambda* (expr-with-pos)
    (define expr (first expr-with-pos))
    (define position (second expr-with-pos))
    (cond ((and (list? expr)
                (>= (length expr) 3)
                (= (first expr) (q define))
                (symbol? (second expr)))
           (do
             (define name (second expr))
             (define body (third expr))
             (define all-symbols (extract-all-symbols body))
             (list name position all-symbols)))
          (else ()))))

(define check-dependency-order
  (lambda* (dependency-graph)
    ; Find definitions that come before their dependencies
    (filter (lambda (entry)
              (define name (first entry))
              (define position (second entry))
              (define deps (third entry))
              
              (any? (lambda (dep)
                      (define dep-entry 
                        (find (lambda (e) (= (first e) dep)) dependency-graph))
                      (and dep-entry
                           (> (second dep-entry) position)))
                    deps))
            dependency-graph)))
```

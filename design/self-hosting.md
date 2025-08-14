# Self-hosting notes

Robert-Fisher_tmemu: @workspace I don't know if I want to do it, but I'm wondering what features would need to be added to Noeval to achieve self-hosting.

Workspace: Collecting workspace informationLooking at your Noeval interpreter, self-hosting would require several significant additions. Let me analyze what you'd need:

## Core Missing Features for Self-Hosting

### 1. **File I/O Operations**

You'd need operatives to read and write files:

```cpp
define_builtin("read-file", builtins::read_file_operative);
define_builtin("write-file", builtins::write_file_operative);
```

### 2. **String Manipulation**

The parser heavily uses string operations:

```cpp
define_builtin("string-append", builtins::string_append_operative);
define_builtin("string-length", builtins::string_length_operative);
define_builtin("string-ref", builtins::string_ref_operative);  // char at index
define_builtin("substring", builtins::substring_operative);
```

### 3. **Character Operations**

For parsing individual characters:

```cpp
define_builtin("char?", builtins::char_p_operative);
define_builtin("char=?", builtins::char_equal_operative);
define_builtin("char->int", builtins::char_to_int_operative);
define_builtin("int->char", builtins::int_to_char_operative);
```

### 4. **Hash Tables/Maps**

Your C++ implementation uses `std::unordered_map` for environments:

```cpp
define_builtin("make-hash-table", builtins::make_hash_table_operative);
define_builtin("hash-ref", builtins::hash_ref_operative);
define_builtin("hash-set!", builtins::hash_set_operative);
```

### 5. **Vector/Array Operations**

For efficient list operations and parameter binding:

```cpp
define_builtin("make-vector", builtins::make_vector_operative);
define_builtin("vector-ref", builtins::vector_ref_operative);
define_builtin("vector-set!", builtins::vector_set_operative);
define_builtin("vector-length", builtins::vector_length_operative);
```

## Parser-Specific Requirements

### 6. **Advanced String Parsing**

Your `parser.cpp` needs:

- Escape sequence handling
- Number parsing (integers, potentially floats)
- Comment processing
- `#skip`/`#end` directive handling

### 7. **Error Handling Enhancement**

More sophisticated error context tracking for parsing errors.

## Architectural Considerations

### 8. **Reflection/Metaprogramming**

To implement the evaluator in Noeval itself:

```noeval
(define eval-in-noeval
  (vau (expr env) calling-env
    ; Would need introspection into value types
    (cond 
      ((symbol? expr) (env-lookup expr env))
      ((cons? expr) (apply-operative (eval (first expr) env) 
                                     (rest expr) env))
      ; ... handle other cases
      )))
```

### 9. **Type Introspection**

```cpp
define_builtin("symbol?", builtins::symbol_p_operative);
define_builtin("cons?", builtins::cons_p_operative);
define_builtin("operative?", builtins::operative_p_operative);
define_builtin("integer?", builtins::integer_p_operative);
```

## Implementation Strategy

Given your fexpr-based architecture, I'd recommend this approach:

### Phase 1: Essential Infrastructure

1. File I/O
2. String manipulation
3. Hash tables
4. Type predicates

### Phase 2: Parser in Noeval

Implement a Noeval-to-Noeval parser that can read `.noeval` files and produce the same AST structure your C++ parser creates.

### Phase 3: Evaluator in Noeval  

This is where fexprs shine - you could implement `eval` as a Noeval operative that dispatches based on expression type.

## The Interesting Question

The fascinating aspect is whether you'd keep the same evaluation model. Your current C++ `eval` function could be reimplemented as:

```noeval
(define noeval-eval
  (vau (expr env) ()
    (cond
      ((symbol? expr) (env-lookup expr env))
      ((integer? expr) expr)
      ((string? expr) expr)
      ((nil? expr) expr)
      ((cons? expr) (apply-fexpr (noeval-eval (first expr) env)
                                 (rest expr) env))
      (else (raise "Cannot evaluate expression")))))
```

The meta-circular nature would be quite elegant with your fexpr foundation.

## Practical Recommendation

Start with file I/O and string operations - you could begin by writing a simple Noeval program that can read and pretty-print `.noeval` files. That would give you immediate feedback on what primitives feel most essential.

Would you be most interested in pursuing the parser-in-Noeval direction first, or the meta-circular evaluator?

Robert-Fisher_tmemu: @workspace I think, at least from a theoretical standpoint, not all of those features would need to be builtins.

`string->list` and `list->string` as builtins could enable all the string manipulation to be done without additional built-ins.

I feel like, with `substring` and `string-append`, having the `char` functions wouldn't strictly be necessary. We could just use single character strings. What do you think `char->int` and `int->char` would be needed for?

An associative container to substitute for `undordered_map` could be built using lists.

While vectors could provide more efficient collection handling, their role could be fulfilled by lists. (And the C++ interpreter may have switched to using `std::vector` for lists under-the-covers by that time.)

Thoughts?

Workspace: Collecting workspace informationYou're absolutely right from a theoretical standpoint! Your minimalist approach is very much in the spirit of fexpr-based languages like Kernel. Let me think through this:

## String Operations as Lists

With `string->list` and `list->string` as primitives, you could indeed implement most string operations:

````cpp
define_builtin("string->list", builtins::string_to_list_operative);
define_builtin("list->string", builtins::list_to_string_operative);
````

Then in Noeval itself:

```noeval
(define string-append 
  (lambda strings
    (list->string 
      (apply append (map string->list strings)))))

(define string-length 
  (lambda (str) 
    (length (string->list str))))

(define substring 
  (lambda (str start end)
    (list->string 
      (take (drop (string->list str) start) 
            (- end start)))))
```

## Character vs Single-Character Strings

You raise an excellent point about `char->int`/`int->char`. Looking at typical use cases:

- **Unicode/encoding work**: Converting between character codes and characters
- **Parsing**: Checking if a character is a digit, letter, etc.
- **Lexical analysis**: Character-by-character processing

But with single-character strings, you could potentially handle these cases with:

```noeval
; Instead of (char->int #\A)
(define char->int 
  (lambda (char-str) 
    (unicode-codepoint char-str)))  ; hypothetical builtin

; Character classification through string operations
(define digit-char? 
  (lambda (char-str)
    (and (= (string-length char-str) 1)
         (>= (char->int char-str) 48)  ; '0'
         (<= (char->int char-str) 57)))) ; '9'
```

So you might only need one primitive: `unicode-codepoint` and `codepoint->string`.

## Associative Containers as Lists

Absolutely! Association lists are a classic Lisp approach:

```noeval
(define assoc 
  (lambda (key alist)
    ((nil? alist) 
     ()
     ((= key (first (first alist)))
      (first alist)
      (assoc key (rest alist))))))

(define alist-set 
  (lambda (key value alist)
    (cons (list key value) alist)))
```

The performance trade-off would be significant for large maps, but for a self-hosting exercise, it's perfectly viable.

## Vectors vs Lists

Your point about lists vs vectors is spot-on. In a self-hosted implementation, you'd likely implement "lists" as vectors under the hood anyway for performance. The surface language could still expose the list interface.

## Minimal Self-Hosting Set

Based on your analysis, the truly essential primitives might be:

### Core String Processing

```cpp
define_builtin("string->list", builtins::string_to_list_operative);
define_builtin("list->string", builtins::list_to_string_operative);
define_builtin("unicode-codepoint", builtins::unicode_codepoint_operative);
define_builtin("codepoint->string", builtins::codepoint_to_string_operative);
```

### File I/O

```cpp
define_builtin("read-file", builtins::read_file_operative);
define_builtin("write-file", builtins::write_file_operative);
```

### Maybe Type Introspection

```cpp
define_builtin("value-type", builtins::value_type_operative);  // returns symbol
```

Everything else could be library functions! This is a much more elegant approach than my initial analysis suggested.

## The Beauty of This Approach

What's particularly elegant is that by the time you implement self-hosting, you'd have:

1. **Proven** that your language design is complete
2. **Validated** that your primitive set is minimal
3. **Demonstrated** the power of fexprs for metaprogramming

The self-hosted parser and evaluator would serve as the ultimate stress test of your language design. If you can write them elegantly in Noeval itself, you've created something truly powerful.

Would you lean toward this minimal primitive approach, or do you think there are practical considerations that would push you toward more built-ins?

Robert-Fisher_tmemu: That hypothetical `unicode-codepoint` function points to one of the reasons I tend to be wary of a character type. Now that UTF-8 everywhere is the *de facto* standard, the old concept of a "character" is much less relevant. We have strings and codepoints, and UTF-8 codepoints come in different lengths.

If we decided to `(define λ lambda*)` or `(define ≤ (lambda (x y) (or (< x y) (= x y))))`, those my look like individual characters, but they're not. And that's before we get into things like combining pairs.

GitHub Copilot: You raise an excellent point about UTF-8 and modern string handling! This actually strengthens the case for your minimalist approach even more.

## The UTF-8 Reality

You're absolutely right that the traditional "character" concept breaks down with UTF-8:

- `λ` is 2 bytes (U+03BB)
- `≤` is 3 bytes (U+2264)
- `🎯` would be 4 bytes (U+1F3AF)
- Combining characters like `é` could be `e` + combining acute accent

## A More UTF-8 Aware Primitive Set

Given this reality, your string-as-list-of-single-character-strings approach becomes even more elegant. But we might want to be explicit about what we mean:

````cpp
// These operate on UTF-8 codepoints, not bytes
define_builtin("string->codepoints", builtins::string_to_codepoints_operative);
define_builtin("codepoints->string", builtins::codepoints_to_string_operative);
define_builtin("codepoint->int", builtins::codepoint_to_int_operative);
define_builtin("int->codepoint", builtins::int_to_codepoint_operative);
````

Then in Noeval:

```noeval
; Convert string to list of single-codepoint strings
(define string->list string->codepoints)
(define list->string codepoints->string)

; Length in codepoints, not bytes
(define string-length 
  (lambda (str) 
    (length (string->list str))))

; Substring by codepoint boundaries
(define substring
  (lambda (str start end)
    (list->string 
      (take (drop (string->list str) start) 
            (- end start)))))
```

## Benefits of This Approach

### 1. **Unicode-Correct by Default**

```noeval
(string-length "λ≤🎯") ; => 3 (not 9 bytes)
(first (string->list "λαμβδα")) ; => "λ"
```

### 2. **No Character Type Confusion**

Everything is either a string or an integer codepoint:

```noeval
(define char->int codepoint->int)
(define int->char int->codepoint) 
```

### 3. **Handles Complex Cases Naturally**

```noeval
; This would work correctly even with combining characters
(define reverse-string 
  (lambda (str)
    (list->string (reverse (string->list str)))))
```

## Implementation Considerations

Your C++ implementation would need UTF-8 handling, but that's becoming standard:

````cpp
// In builtins namespace
value_ptr string_to_codepoints_operative(const std::vector<value_ptr>& args, env_ptr env)
{
    // ... validation ...
    auto str = std::get<std::string>(eval(args[0], env)->data);
    
    // Convert UTF-8 string to vector of codepoint strings
    value_ptr result = std::make_shared<value>(nullptr); // nil
    
    // Iterate through UTF-8 codepoints (using C++23/26 text facilities)
    for (auto codepoint : str | std::views::as_utf8) {
        std::string single_cp;
        // Convert back to UTF-8 string
        // ...
        auto cp_val = std::make_shared<value>(single_cp);
        result = std::make_shared<value>(cons_cell{cp_val, result});
    }
    
    return reverse_list(result); // Helper to reverse the cons list
}
````

## The Elegance

This approach means your self-hosted Noeval would handle Unicode correctly from day one, without needing special character handling logic. The parser could even allow Unicode identifiers naturally:

```noeval
(define λ (vau (params body) env 
           (eval (cons 'vau (cons params (cons env (cons body ())))) env)))

(define ≤ (lambda (x y) (or (< x y) (= x y))))

((λ (x y) (≤ x y)) 3 5) ; => true
```

## Minimal Self-Hosting Set (Revised)

```cpp
// String/Unicode operations
define_builtin("string->codepoints", builtins::string_to_codepoints_operative);
define_builtin("codepoints->string", builtins::codepoints_to_string_operative);
define_builtin("codepoint->int", builtins::codepoint_to_int_operative);
define_builtin("int->codepoint", builtins::int_to_codepoint_operative);

// File I/O (UTF-8 aware)
define_builtin("read-file", builtins::read_file_operative);
define_builtin("write-file", builtins::write_file_operative);
```

Everything else - string manipulation, parsing, even complex Unicode operations - could be library functions built on these primitives.

Your instinct to avoid a character type was spot-on. UTF-8 strings + codepoint integers is a much cleaner model for modern text processing, and it keeps your primitive set minimal while being Unicode-correct by design.

# syntax-rules

Notes toward implementing `syntax-rules` on top of Noeval's macros (see
[macros.md](macros.md)). Nothing here is implemented. The work is on hold,
and when it's picked up it may be only a proof of concept, which might mean
keeping it out of `src/lib.noeval` (see [Where it lives](#where-it-lives)).

## What fits easily

- **No `define-syntax`.** `syntax-rules` can be a library operative that
  returns a macro value, so it's used with `define`:

  ```scheme
  (define my-when
    (syntax-rules ()
      ((_ test body ...) (if test (do body ...)))))
  ```

  `let-syntax` and `letrec-syntax` come free: they're `let` with macro
  values.
- **A library implementation is fast enough.** A transformer runs once per
  combination, since the expansion is cached, so matching and filling in
  templates in Noeval code is fine. That keeps with minimizing primitives.
- **The reader already accepts `...` and `_` as symbols** (checked).
- **Dotted and vector patterns are left out.** Noeval has neither improper
  lists nor vectors. `(_ x rest ...)` covers what `(_ x . rest)` would do.
- **The first element of a pattern is ignored.** A transformer receives only
  the operands, not the operator, so the matcher matches `(rest pattern)`
  against them. That's equivalent to R7RS ignoring the keyword position.
- **Pattern literals are compared by name**, as `cond` already compares
  `else`, rather than by binding (`free-identifier=?`), since there are no
  syntax objects.
- **Datum literals need a type check.** `=` raises an error for most pairs of
  different types (`(= 5 (q a))` fails), so the matcher has to compare
  `typeof` first.
- **Patterns can be checked once**, when `syntax-rules` is evaluated: where
  ellipses appear, duplicate pattern variables, and whether each pattern
  variable is used in templates at the ellipsis depth it has in its pattern.
- **No rule matching** raises an error naming the macro's operands. The error
  is reported at the call's location, as for any macro.

## Symbols in templates

This is the hard part. A template is written with names (`if`, `do`, `tmp`),
but Noeval's hygiene convention is to embed values rather than names. A
Scheme expander decides what each identifier in a template is by knowing the
core forms. Noeval can't: any operative may treat an operand as a binding, as
code, or as data. So a template can't be analyzed structurally, and the rule
has to be decided symbol by symbol.

### Proposed rule

For each symbol in a template that isn't a pattern variable, `...`, or `_`:

1. **If it's bound in the macro's definition environment, embed its value.**
   The lookup is made at expansion time, through the transformer's closure
   over the environment `syntax-rules` was evaluated in, not when
   `syntax-rules` is evaluated. So a macro can refer to itself (a recursive
   `my-or`) and to things defined after it. That's how the existing macros
   get the values of `if` and `do`. Evaluating a symbol fails only when it's
   unbound, so a `try` around `eval` tells the two cases apart, as
   `define-keyword` does.
2. **Otherwise, rename it** with `gensym`, using the same new name for every
   occurrence in one expansion. That makes introduced temporaries hygienic:

   ```scheme
   (define my-or
     (syntax-rules ()
       ((_) false)
       ((_ e) e)
       ((_ e r ...) (let ((tmp e)) (if tmp tmp (my-or r ...))))))
   ```

   expands with the values of `let`, `if`, and `my-or`, and a new name in
   place of `tmp`, so `(let ((tmp 5)) (my-or false tmp))` is 5.

Pattern variables are replaced with the user's operands as they are, so the
user's symbols are looked up (or bound) in the calling environment.

This gets both halves of hygiene in the common cases: names the macro refers
to can't be captured by the caller's bindings, and names the macro binds
can't capture the caller's.

### Where it breaks

- **A binding name that happens to be bound in the definition environment.**
  `(let ((list e)) ...)` in a template embeds `list`'s value where `let`
  expects a name. It fails loudly ("let binding's name must be a symbol"), so
  the macro's author can pick another name, but a hygienic expander would
  have renamed it.
- **Symbols an operative uses by name**: `else` for `cond`, the contents of a
  `q`, the names given to `define-keyword`, an anaphoric `it`, or a caller's
  variable referred to on purpose (the `counter` example in
  [macros.md](macros.md#hygiene)). Renaming breaks all of them. Partial fixes:
  - Leave the contents of a `q` form alone, apart from substituting pattern
    variables: a subtemplate whose operator symbol resolves to `q`'s value.
    That gives the same result as Scheme, where quoting strips the renaming.
  - Define `else` to evaluate to itself, as keywords do. Rule 1 then embeds
    the symbol `else`, and `cond`, which compares by name, is unaffected.
  - Keywords (`:name`) already evaluate to themselves, so rule 1 inserts them
    unchanged as long as they're defined.

  Since fexprs can take any operand as data, there will always be cases like
  these, so a macro needs a way to insert a symbol as it is. See the open
  questions.
- **Mutable bindings.** A cached expansion holds a value, not a binding. A
  template that refers to a `define-mutable` variable gets its value at
  expansion time, won't see later `set!`s, and `(set! counter ...)` receives
  a number instead of a name. For immutable bindings the value is as good as
  the binding, since `define` can't rebind a name. Library code can't tell
  whether a binding is mutable. Embedding an operative that looks the name up
  in the definition environment would fix reads but not `set!`.
- **Stale decisions.** Whether a symbol is bound is decided when a
  combination is first expanded. A template symbol meant as a temporary that
  is later defined in the definition environment is renamed at call sites
  expanded before the definition and embedded at those expanded after it.
  The existing macros share the underlying property (a cached expansion keeps
  the values it embedded), but here it changes the expansion's meaning.

## syntax-case

`TODO.md` pairs `syntax-case` with `syntax-rules`. Without syntax objects, it
would be a procedural transformer that uses the same pattern matcher, with a
`syntax` form that fills in a template by the same rules. So `syntax-rules`
should be built from two reusable functions, a matcher (operands and pattern
to bindings of pattern variables, or no match) and a template filler
(template and bindings to an expansion), and `syntax-case` left for later.

## Where it lives

As a proof of concept, `syntax-rules` doesn't need to be in `src/lib.noeval`.
It could be a separate file that's loaded on demand, which keeps its helpers
and its cost out of every program's startup. Without a module system,
though, loading it adds its helpers' names to the environment it's loaded
into, and each loaded name can only be defined once there. A module system
(see `TODO.md`) would let it export only `syntax-rules`, so this may be a
reason to build one first.

The library's own macros shouldn't be rewritten with `syntax-rules` either
way: it would slow down loading the library, and the hand-written
transformers give more specific error messages.

## Tests

- Port `infix`, whose Scheme `syntax-rules` source is in its comment in
  `src/lib.noeval`, and check it against the hand-written version.
- `my-or` with a `tmp` temporary, against a user's own `tmp`.
- `swap!`, whose temporary has to be bound alongside the user's code.
- A recursive macro, and one that refers to a function defined after it.
- A local `if` or `do` in the calling environment doesn't affect an
  expansion.
- Literals, datum literals of different types, `_`, nested ellipses, and
  elements after an ellipsis.
- Errors: malformed patterns, a pattern variable used at the wrong ellipsis
  depth, and no rule matching.

## Open questions

1. **The template symbol rule.** Is "bound means embed the value, unbound
   means rename," looked up in the definition environment at expansion time,
   the right default? (The recommendation.)
2. **Inserting a symbol as it is.** Options:
   - Reuse the literals list, so `(syntax-rules (else it) ...)` means "match
     these by name, and insert them by name." These are the names the macro
     treats by name anyway. (The preferred option.)
   - Always insert keywords as they are, bound or not.
   - An explicit template form.
3. **Mutable bindings.** Document the limitation, rely on the answer to
   question 2, or add a primitive that tells whether a binding is mutable?
4. **How much of R7RS.** Elements after an ellipsis, `(a ... b c)`, are cheap
   and worth including. A custom ellipsis name
   (`(syntax-rules ::: (literals) ...)`) and the `(... ...)` escape matter
   mostly for macros that define macros, and could wait. A template that
   contains a `syntax-rules` would also have its symbols embedded or renamed
   by the outer macro, which needs thought of its own.

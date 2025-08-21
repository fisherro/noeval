# Sequences

## Common Lisp

Has a sequence type that encompasses lists and vectors. Vectors include the
specialized string and bit-vector types.

There is a big library of generic sequence functions, but these only work for
the built-in types. It is not extensible to user-created types.

## Racket

Has a generic sequence protocol. Anything that implements it can be iterated
using common syntax/functions.

Racket sequence protocol: A thunk that returns six values...

* `pos->element` procedure
* `next-pos` procedure
* `init-pos` value
* `continue-with-pos?` function or `#f`
* `continue-with-val?` function or `#f`
* `continue-after-pos+val?` function or `#f`

A `prop:sequence` can provide a lambda to convert the object with the property
to a sequence.

Objects implementing the stream generic interface can also be treated as
sequences. (It isn't clear to me how much this is actually part of the
"protocol" rather than just an adaptor like `prop:sequence`.)

Racket stream generic interface: `stream-empty?`, `stream-first`, `stream-rest`

The protocol allows many hooks versus the generic interface having one
contract.

The sequence protocol pre-dates the generic interface machinery.

The protocol doesn't require a type...it can be completely *ad hoc*.

The protocol can be extremely light and optimized easier than generic
interfaces.

## Clojure

Sequences (seqs) are a foundational concept.

A seq is an immutable, persistent, uniform view with first/rest/cons. They are
values.

Whereas in Racket sequences are not usually stored or passed around, just
consumed.

Clojure seqs are more like Racket streams than like Racket sequences.

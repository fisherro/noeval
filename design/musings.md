# Noeval musings

I knew that drawing the line between keeping the core conceptually minimal
while making concessions to practicality was hard. This project has highlighted
for me that it is harder than I thought going into it.

> Programming languages should be designed not by piling feature on top of
feature, but by removing the weaknesses and restrictions that make additional
features appear necessary.

This statement from R3RS is *possibly* a guiding light.

One thing to note is its mention of *weaknesses* and *restrictions*. According
to the statement, adding a feature to address efficiency is fine provided it
doesn't create weaknesses or restrictions.

## On efficiency features

### The good

A feature added for efficiency that exposes a general semantic capability can
be a good thing. e.g. Vectors and bytevectors unlock capabilities the language
might not have without them, which is directly related to efficiency.

A feature that reveals user intent to the implementation can benefit
optimization in a way that a user-level encoding might hide it.

### The bad

Efficiency features that duplicate existing semantics narrowly can be bad.
(I need a good example of this.)

Efficiency features that leak implementation details can be bad.

Efficiency features that create semantic interference with and interact badly
with other features can be bad.

## On Church numerals

Using binary numerals instead of Church numerals isn't really adding a feature
for efficiency. They're a workaround for a self-imposed limitation. They don't
really bring anything to the table.

Church numerals conflate data representation with computation strategy, which
violates separation of concerns.

Using Church numerals signs up for the weakness of disguising arithmetic as
recursion/iteration. Having numbers in the core isn't a weakness but a
strength.

## On Church Booleans

Unlike Church Booleans, these may be adding strength.

Eliminates a primitive with little cost.

May need to ensure the optimizer can "see through" them to emit a real branch
instruction, eliminate dead-code, or preserve tail-position guarantees. The
optimizer needs to know and be guaranteed that only one branch will be
evaluated.

We lose the ability to treat generic values as true or false as some languages
do. But that can arguably be a strength rather than a weakness.

Booleans may need to be distinguishable from general fexprs.

## On strings

I acknowledge up-front that programmers have to understand issues about text
that end-users do not. No language or library is going to effectively hide the
complications of grapheme clusters, normalizations, etc. from the programmer.

Sequences of numbers representing Unicode codepoints appears to be the right
level of abstraction for the programmer. The trouble is that sequences of
numbers are the least efficient way to deal with Unicode strings. Which
suggests the need for a string-specific type.

This raises two issues:

* Strings are sequences, so they need the same capabilities as sequences.
  Having two types with so much overlap increases the complexity of the
  implementation and programmer cognitive load.
* Other sequences have special needs too. When do we decide that these needs
  rise to the level of deserving a language specific feature.

The first issue can, perhaps, be addressed by having a way to present the same
interface for both sequences and strings. Strings simply get a superset of the
sequence capabilities.

One solution might be...

* The language needs a primitive byte type
* The language needs a primitive sequence type that serves the needs of strings
* The language needs a generic sequence interface that a library implementation
  of strings can leverage to provide a codepoint-level view on a byte-level
  sequence

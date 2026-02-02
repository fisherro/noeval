# Garbage collection musings

The circular references possible in Noeval are limited:

* While cons cells could potientially create cycles, the Noeval language does not provide a way to do it. (It could be done in the C++ code, but we can just not do that.)
* While mutable bindings might also look like a way to produce cycles, the language does not allow it.
* An environment could contain a reference to itself. Likewise cycles of references between environments are possible.
* Since operatives contain a closure environment, they can participate in such cycles as well.

Ephemeral environments or operatives in the C++ code could be missed by the collector.
To address this, we created `env_root_ptr`, which adds such environment pointers to the GC roots.
It requires some discipline in writing the C++ code to guaranteed these are used where necessary.
No provision is made for ephemeral operatives.

## Values

The current value consists of:

```c++
    std::variant<
        bignum,
        std::string,
        symbol,
        cons_cell,
        operative,
        builtin_operative,
        env_ptr,
        mutable_binding,
        eof_object,
        std::nullptr_t  // for nil
    > data;
```

* Bignums can allocate memory, but they cannot contain pointers to other values.
* Strings can allocate memory, but they cannot contain pointers to other values.
* Symbols are implemented as strings (see above).
* Cons cells contain pointers to two other values.
* Operatives contain an environment, which can (of course) point to other values.
* Built-in operatives: The callable this holds could potentially hold pointers to other values. (But they do not in practice.)
* Environments contain symbols and values. (But not pointers?)
* Mutable bindings contain values. (But not pointers?)
* The EOF object and NULL do not contain pointers to other values.

Currently all values are managed by `shared_ptr`. This is used to enable the recursive nature of the `value`. But values are, generally, treated as value types. Only environments are treated as reference types.

## Some thoughts about GC implementation

* Provide a memory resource that can track pointers embedded in standard containers?
* All garbage collected objects derive from `collectible`.
* Collectible

```c++
struct collectible {
    inline static std::set<collectible> registry;
    bool mark{false};
    std::set<collectible> kids;
};
```

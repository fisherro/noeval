# Environment garbage collection

## Background

We use `std::shared_ptr` for management of Noeval values and environments.

We don't allow the creation of cycles in Noeval code with `cons_cell`.

The child->parent relationship for environments cannot have cycles.

Environments be values bound in environments, however, and this can create
cycles.

Our root is the `global_env` on the stack in `main`. This can get a different
pointer assigned to it, but all live environments must be referenced from the
current value of that `global_env`.

An environment could be referenced by a value through...

* `env_ptr`
* `cons_cell::car`
* `cons_cell::cdr`
* `operative::closure_env`
* `operative::body`?
* `mutable_binding::value`
* Nested references within any of those

## Strategy

Using `std::weak_ptr` is not easy because there is not always a clear candidate
for the weak pointer.

The `env_ptr` will continue to be a `std::shared_ptr`.

There will be a global environment registry, which will be a
`std::vector<std::weak_ptr<environment>>`.

Every environment needs to add itself to the global registry. This could be done
from a factory method while making the ctor private.

Then, when we want to collect, we:

1. Mark live environments by recursively searching from the root. When recursing stop recursing when we reach a pointer we've already marked.
2. For each unmarked environment in the registry, clear its bindings and reset its parent pointer.
3. Remove expired pointers from the registry.

Collection should only be done between evaluations.

# Using `std::indirect`

Currently, `value` objects are managed by `std::shared_ptr` to enable the recursive nature of `value`.

Here's a simple example of the recursive variant issue:

```c++
struct cons_cell {
    value car;
    value cdr;
};

using value = std::variant<cons_cell, ...>;
```

A `cons_cell` contains two `value` objects, but `value` cannot be defined without referencing `cons_cell`. We get around this by using `std::shared_ptr<value>`, which can be forward declared.

```c++
struct value;

struct cons_cell {
    std::shared_ptr<value> car;
    std::shared_ptr<value> cdr;
};

using value = std::variant<cons_cell, ...>;
```

The `std::indirect` class template from C++26 provides a more natural way to handle this.

```c++
struct value;

struct cons_cell {
    std::indirect<value> car;
    std::indirect<value> cdr;
};

using value = std::variant<cons_cell, ...>;
```

## Uses of `value_ptr`

* Tail call & continuation
* Cons cell
* `operative::body`
* Mutable bindings
* Environments

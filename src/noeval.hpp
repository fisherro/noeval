#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

using bignum = boost::multiprecision::cpp_rational;

// Forward declarations
struct environment;
struct value;

using value_ptr = std::shared_ptr<value>;
using env_ptr = std::shared_ptr<environment>;

// Used to ensure that environments referenced only by the C++ code do not get
// collected.
class env_root_ptr final {
    env_ptr env;
public:
    explicit env_root_ptr(env_ptr e);
    ~env_root_ptr();
    env_ptr get() const { return env; }
    env_ptr operator->() const { return env; }
    operator bool() const { return static_cast<bool>(env); }

    // Copy operations
    env_root_ptr(const env_root_ptr& that);
    env_root_ptr& operator=(const env_root_ptr& that);

#if 0
    // Add move constructor and assignment
    env_root_ptr(env_root_ptr&& that) noexcept: env(std::move(that.env))
    {
        that.env = nullptr; // Prevent destructor from removing
    }
    
    env_root_ptr& operator=(env_root_ptr&& that) noexcept
    {
        if (this != &that) {
            if (env) environment::remove_root(env);
            env = std::move(that.env);
            that.env = nullptr;
        }
        return *this;
    }
#endif
};

// Tail call captures the eval arguments for the next iteration of eval when
// a tail call happens.
struct tail_call {
    value_ptr    expr;
    env_root_ptr env;
};

// Functions that eval calls that could result in a tail call will use this
// return type. If it contains a tail_call, then eval should loop around using
// those values as its new arguments. Otherwise, this will just hold a return
// value.
using continuation_type = std::variant<tail_call, value_ptr>;

// Core value types
struct symbol {
    std::string name;
    explicit symbol(std::convertible_to<std::string_view> auto&& n):
        name{std::forward<decltype(n)>(n)} {}
    std::string to_string() const { return name; }
    bool operator==(const symbol& that) const { return name == that.name; }
};

struct cons_cell {
    value_ptr car;
    value_ptr cdr;
    cons_cell(value_ptr a, value_ptr d) : car(std::move(a)), cdr(std::move(d)) {}
    std::string to_string() const;
    bool operator==(const cons_cell& that) const;
};

struct param_pattern {
    bool is_variadic{false};
    // If variadic, param_names will only have a single name for the rest parameter.
    std::vector<std::string> param_names;
};

struct operative {
    param_pattern params;
    std::string env_param;
    value_ptr body;
    env_ptr closure_env;
    std::string tag;
    
    operative(param_pattern p, std::string e, value_ptr b, env_root_ptr env,
        std::string_view t = "")
        : params(std::move(p)), env_param(std::move(e)),
          body(std::move(b)), closure_env(std::move(env.get())), tag(t) {}

    std::string to_string() const;
    bool operator==(const operative& that) const
    {
        if ((not tag.empty()) and tag == that.tag) return true;
        return false;
    }
};

// Built-in operative type for primitives
struct builtin_operative {
    std::string name;
    std::function<continuation_type(const std::vector<value_ptr>&, env_root_ptr)> func;

    builtin_operative(std::string n, std::function<continuation_type(const std::vector<value_ptr>&, env_root_ptr)> f)
        : name(std::move(n)), func(std::move(f)) {}
    std::string to_string() const { return "#<builtin-operative:" + name + ">"; }
    bool operator==(const builtin_operative&) const { return false; }
};

// Add a mutable wrapper type
struct mutable_binding {
    value_ptr value;
    explicit mutable_binding(value_ptr v) : value(std::move(v)) {}
    std::string to_string() const;
    bool operator==(const mutable_binding& that) const
    { return value == that.value; }
};

struct eof_object {
    std::string to_string() const { return "#<eof-object>"; }
    bool operator==(const eof_object&) const { return true; }
};

// The main value type
/*
We could use Church encoding for integers, but the performance overhead and
not using the processor's native support means handling numbers directly makes
more sense.

We could also use Church encoding for cons cells, but--likewise--this has
impractical performance overhead.
*/
struct value: std::enable_shared_from_this<value> {
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

private:
#if 0
    static inline int object_count{0};
    template<typename T>
    value(T&& t): data(std::forward<T>(t)) { ++object_count; /*std::println("objects: {}", object_count);*/ }
public:
    ~value() { --object_count; std::println("objects: {}", object_count); }
#else
    template<typename T>
    value(T&& t): data(std::forward<T>(t)) {}
#endif

public:
    template<typename T>
    static std::shared_ptr<value> make(T&& v)
    {
        return std::shared_ptr<value>(new value(std::forward<T>(v)));
    }

    static std::shared_ptr<value> make(env_root_ptr env)
    {
        return value::make(env.get());
    }

    friend bool operator==(value& lhs, value& rhs);
};

struct typeof_visitor {
    std::string operator()(const bignum&) const { return "number"; }
    std::string operator()(const std::string&) const { return "string"; }
    std::string operator()(const symbol&) const { return "symbol"; }
    std::string operator()(const cons_cell&) const { return "cons-cell"; }
    std::string operator()(const operative&) const { return "operative"; }
    std::string operator()(const builtin_operative&) const { return "operative"; }
    std::string operator()(env_ptr) const { return "environment"; }
    std::string operator()(const mutable_binding& mb) const;
    std::string operator()(const eof_object&) const { return "eof-object"; }
    std::string operator()(std::nullptr_t) const { return "nil"; }
};

// Environment for variable bindings
struct environment final {
private:
    // Keep a count of all constructed (& not destructed) environments for debugging
    static inline size_t count{0};

    // Registry of all environments used for garbage collection
    // weak_ptr can't be used with unordered_set until owner_hash is implemented
    static inline std::set<std::weak_ptr<environment>, std::owner_less<std::weak_ptr<environment>>> registry;
    // Roots with reference counts:
    static inline std::map<std::weak_ptr<environment>, size_t, std::owner_less<std::weak_ptr<environment>>> roots;

    std::unordered_map<std::string, value_ptr> bindings;
    env_ptr parent;

    // Private ctor; must use environment::make to create instances
    environment(env_ptr p = nullptr) : parent(std::move(p)) { ++count; }

    static void cleanup_registry();
    static std::unordered_set<environment*> mark();
    static void mark_value(std::unordered_set<environment*>& marked, value* v);
    static void mark_environment(std::unordered_set<environment*>& marked, environment* env);
    static void sweep(std::unordered_set<environment*>& marked);

public:
    static void collect();
    static size_t get_constructed_count() { return count; }
    static size_t get_registered_count() { return registry.size(); }
    static void dump_roots();
    static void add_root(env_ptr env);
    static void remove_root(env_ptr env);
    static std::vector<std::string> get_root_symbols();

    static env_root_ptr make();
    static env_root_ptr make(env_ptr parent);
    static env_root_ptr make(env_root_ptr parent);

    ~environment() { --count; }

    value_ptr lookup(const std::string& name) const;
    void define(const std::string& name, value_ptr val);
    std::vector<std::string> get_all_symbols() const;
    std::string dump_chain() const;
};

// Custom exception class with context
class evaluation_error: public std::runtime_error {
public:
    std::string message;
    std::string context;
    std::string stack_trace;
    
    evaluation_error(
        const std::string& msg,
        const std::string& ctx = "",
        const std::string& stack = "")
        : std::runtime_error(format_message(msg, ctx, stack)),
          message(msg),
          context(ctx),
          stack_trace(stack) {}

private:
    static std::string format_message(
        const std::string& msg,
        const std::string& ctx,
        const std::string& stack)
    {
        std::string message = msg;
        if (!ctx.empty()) message += "\n while evaluating: " + ctx;
        if (!stack.empty()) message += "\n stack trace:\n" + stack;
        return message;
    }
};

// Helper functions
std::string value_to_string(const value_ptr& val);
std::string value_type_string(const value_ptr& val);
std::string expr_context(const value_ptr& expr);

// List operations
bool is_nil(const value_ptr& val);
bool is_cons(const value_ptr& val);
value_ptr car(const value_ptr& val);
value_ptr cdr(const value_ptr& val);
std::vector<value_ptr> list_to_vector(value_ptr list);

// Core evaluation functions
value_ptr eval(value_ptr expr, env_root_ptr env);
value_ptr top_level_eval(value_ptr expr, env_root_ptr env);
env_root_ptr create_global_environment();
// This creates a new global environment, loads the library, and runs the
// library tests if specified.
env_root_ptr reload_global_environment(bool test_the_library = true);

// String conversion functions
std::string to_string(const bignum& value);
std::string to_string(const std::string& value);
std::string to_string(std::nullptr_t);
std::string to_string(const env_ptr& env);
std::string to_string(const env_root_ptr& env);

void   call_stack_reset_max_depth();
size_t call_stack_get_max_depth();
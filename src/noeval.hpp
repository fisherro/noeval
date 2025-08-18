#pragma once

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

#include <boost/multiprecision/cpp_int.hpp>

using bignum = boost::multiprecision::cpp_rational;

// Forward declarations
struct environment;
struct value;

using value_ptr = std::shared_ptr<value>;
using env_ptr = std::shared_ptr<environment>;

// Tail call captures the eval arguments for the next iteration of eval when
// a tail call happens.
struct tail_call {
    value_ptr expr;
    env_ptr   env;
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
    
    operative(param_pattern p, std::string e, value_ptr b, env_ptr env,
        std::string_view t = "")
        : params(std::move(p)), env_param(std::move(e)),
          body(std::move(b)), closure_env(std::move(env)), tag(t) {}

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
    std::function<continuation_type(const std::vector<value_ptr>&, env_ptr)> func;

    builtin_operative(std::string n, std::function<continuation_type(const std::vector<value_ptr>&, env_ptr)> f)
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
        std::nullptr_t  // for nil
    > data;
    
    template<typename T>
    value(T&& t) : data(std::forward<T>(t)) {}
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
    std::string operator()(std::nullptr_t) const { return "nil"; }
};

// Environment for variable bindings
struct environment {
    std::unordered_map<std::string, value_ptr> bindings;
    env_ptr parent;
    
    environment(env_ptr p = nullptr) : parent(std::move(p)) {}
    
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
value_ptr eval(value_ptr expr, env_ptr env);
env_ptr create_global_environment();
// This creates a new global environment, loads the library, and runs the
// library tests if specified.
env_ptr reload_global_environment(bool run_tests = true);

// String conversion functions
std::string to_string(const bignum& value);
std::string to_string(const std::string& value);
std::string to_string(const env_ptr& env);
std::string to_string(std::nullptr_t);

void   call_stack_reset_max_depth();
size_t call_stack_get_max_depth();
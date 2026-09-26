#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
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

// Tail call captures the eval arguments for the next iteration of eval when
// a tail call happens.
struct tail_call {
    value_ptr    expr;
    env_ptr env;
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

// Where in a source file an expression came from. The parser records one for
// each list it reads, so that errors can report where the failing expression
// is. Lists built at runtime have none.
struct source_location {
    // An interned file name (see intern_file_name), or null if there is no
    // location.
    const std::string* file{nullptr};
    std::uint32_t line{0};
    std::uint32_t column{0};

    explicit operator bool() const { return nullptr != file; }
    std::string to_string() const;
};

// File names live for the life of the program, so a source_location can
// point to one without owning it.
const std::string* intern_file_name(std::string_view name);

struct cons_cell {
    value_ptr car;
    value_ptr cdr;
    // Not part of the cell's value, so operator== ignores it.
    source_location location;
    cons_cell(value_ptr a, value_ptr d, source_location loc = {}):
        car(std::move(a)), cdr(std::move(d)), location(loc) {}
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
        std::string_view t = ""):
        params(std::move(p)), env_param(std::move(e)),
        body(std::move(b)),
        closure_env(std::move(env)),
        tag(t) {}

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

    builtin_operative(std::string n, std::function<continuation_type(const std::vector<value_ptr>&, env_ptr)> f):
        name(std::move(n)), func(std::move(f)) {}
    std::string to_string() const { return "#<builtin-operative:" + name + ">"; }
    bool operator==(const builtin_operative&) const { return false; }
};

// Add a mutable wrapper type
struct mutable_binding {
    value_ptr value;
    explicit mutable_binding(value_ptr v): value(std::move(v)) {}
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
struct environment final: std::enable_shared_from_this<environment> {
private:
    // Keep a count of all constructed (& not destructed) environments for debugging
    static inline size_t count{0};

    // Collection scheduling: We collect when the number of environments created
    // since the last collection reaches the number that survived the last
    // collection (but no fewer than min_collection_interval). In stress mode,
    // we instead collect every stress_interval environments (1 means every
    // time an environment is created).
    static constexpr size_t min_collection_interval{1000};
    static inline size_t created_since_collection{0};
    static inline size_t survivors{0};
    static inline size_t stress_interval{0}; // 0 means stress mode is off
    static void maybe_collect();

    // Registry of all live environments used for garbage collection.
    // Environments add and remove themselves.
    static inline std::unordered_set<environment*> registry;

    std::unordered_map<std::string, value_ptr> bindings;
    env_ptr parent;

    // Private ctor; must use environment::make to create instances
    environment(env_ptr p = nullptr): parent(std::move(p))
    {
        ++count;
        registry.insert(this);
    }

    // The garbage collector (in noeval.cpp) needs access to the bindings and
    // parent.
    friend struct cycle_collector;

public:
    static void collect();
    static void set_stress_interval(size_t interval) { stress_interval = interval; }
    static size_t get_stress_interval() { return stress_interval; }
    static size_t get_constructed_count() { return count; }
    static size_t get_registered_count() { return registry.size(); }

    static env_ptr make();
    static env_ptr make(env_ptr parent);

    ~environment()
    {
        --count;
        registry.erase(this);
    }

    value_ptr lookup(const std::string& name) const;
    void define(const std::string& name, value_ptr val);
    // The names bound in this environment and all its ancestors
    std::vector<std::string> get_all_symbols() const;
    // The names bound in this environment itself, sorted
    std::vector<std::string> get_own_symbols() const;
    env_ptr get_parent() const { return parent; }
    std::string dump_chain() const;
};

// The location of the innermost expression being evaluated that has one, as
// "file:line:column", or empty if none does.
std::string current_source_location();

// Custom exception class with context
class evaluation_error: public std::runtime_error {
public:
    std::string message;
    std::string context;
    std::string stack_trace;
    std::string location;
    
    // The location defaults to where evaluation is when the error is thrown.
    evaluation_error(
        const std::string& msg,
        const std::string& ctx = "",
        const std::string& stack = "",
        const std::string& loc = current_source_location()):
        std::runtime_error(format_message(msg, ctx, stack, loc)),
        message(msg),
        context(ctx),
        stack_trace(stack),
        location(loc) {}

private:
    static std::string format_message(
        const std::string& msg,
        const std::string& ctx,
        const std::string& stack,
        const std::string& loc)
    {
        std::string message = loc.empty()? msg: loc + ": " + msg;
        if (not ctx.empty()) message += "\n while evaluating: " + ctx;
        if (not stack.empty()) message += "\n stack trace:\n" + stack;
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
value_ptr top_level_eval(value_ptr expr, env_ptr env);
env_ptr create_top_level_environment();
// This creates a new top-level environment, loads the library, and runs the
// library tests if specified.
env_ptr reload_top_level_environment(bool test_the_library = true);
// Parse and evaluate each expression in a file, returning the value of the
// last one. A relative filename is resolved against the directory of the file
// currently being loaded, if there is one.
value_ptr load_file(const std::string& filename, env_ptr env);

// String conversion functions
std::string to_string(const bignum& value);
std::string to_string(const std::string& value);
std::string to_string(std::nullptr_t);
std::string to_string(const env_ptr& env);

void   call_stack_reset_max_depth();
size_t call_stack_get_max_depth();
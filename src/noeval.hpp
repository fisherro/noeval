#pragma once

#include <functional>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

// Forward declarations
struct environment;
struct value;

using value_ptr = std::shared_ptr<value>;
using env_ptr = std::shared_ptr<environment>;

// Token types for lexical analysis
enum class token_type {
    left_paren,
    right_paren,
    symbol,
    integer,
    string_literal,
    eof
};

std::string token_type_to_string(token_type type);

struct token {
    token_type type;
    std::string value;
    
    token(token_type t, std::string v = "");
    std::string to_string() const;
};

class lexer {
public:
    explicit lexer(std::string text);
    token next_token();

private:
    std::string input;
    size_t pos;
    
    bool matches_keyword(const std::string& keyword);
    void skip_disabled_block();
    void skip_whitespace_and_comments();
    std::string read_symbol();
    std::string read_string();
};

// Core value types
struct symbol {
    std::string name;
    explicit symbol(std::convertible_to<std::string_view> auto&& n):
        name{std::forward<decltype(n)>(n)} {}
    std::string to_string() const { return name; }
};

struct cons_cell {
    value_ptr car;
    value_ptr cdr;
    cons_cell(value_ptr a, value_ptr d) : car(std::move(a)), cdr(std::move(d)) {}
    std::string to_string() const;
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
    
    operative(param_pattern p, std::string e, value_ptr b, env_ptr env)
        : params(std::move(p)), env_param(std::move(e)), 
          body(std::move(b)), closure_env(std::move(env)) {}
    
    std::string to_string() const;
};

// Built-in operative type for primitives
struct builtin_operative {
    std::string name;
    std::function<value_ptr(const std::vector<value_ptr>&, env_ptr)> func;
    
    builtin_operative(std::string n, std::function<value_ptr(const std::vector<value_ptr>&, env_ptr)> f)
        : name(std::move(n)), func(std::move(f)) {}
    std::string to_string() const { return "#<builtin-operative:" + name + ">"; }
};

// Add a mutable wrapper type
struct mutable_binding {
    value_ptr value;
    explicit mutable_binding(value_ptr v) : value(std::move(v)) {}
    std::string to_string() const;
};

// The main value type
/*
We could use Church encoding for integers, but the performance overhead and
not using the processor's native support means handling numbers directly makes
more sense.

We could also use Church encoding for cons cells, but--likewise--this has
impractical performance overhead.
*/
struct value {
    std::variant<
        int,
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
};

// Environment for variable bindings
struct environment {
    std::unordered_map<std::string, value_ptr> bindings;
    env_ptr parent;
    
    environment(env_ptr p = nullptr) : parent(std::move(p)) {}
    
    value_ptr lookup(const std::string& name) const;
    void define(const std::string& name, value_ptr val);
};

class parser {
public:
    explicit parser(std::string input);
    value_ptr parse_expression();
    value_ptr parse();
    std::vector<value_ptr> parse_all();

private:
    lexer lex;
    token current_token{token_type::eof};
    
    void advance();
    value_ptr parse_list();
};

// Custom exception class with context
class evaluation_error: public std::runtime_error {
public:
    std::string original_message;
    std::string context;
    
    evaluation_error(const std::string& msg, const std::string& ctx = "") 
        : std::runtime_error(format_message(msg, ctx)), original_message(msg), context(ctx) {}
    
private:
    static std::string format_message(const std::string& msg, const std::string& ctx)
    {
        if (ctx.empty()) {
            return msg;
        }
        return msg + "\n  while evaluating: " + ctx;
    }
};

// Debug system
extern std::unordered_map<std::string, std::string> debug_categories;

class debug_controller {
private:
    std::unordered_set<std::string> enabled_categories;
    bool use_colors = true;
    
public:
    void enable(const std::string& category)
    { enabled_categories.insert(category); }

    void disable(const std::string& category)
    { enabled_categories.erase(category); }

    auto get_enabled_categories() const
    { return enabled_categories; }

    void set_enabled_categories(const std::unordered_set<std::string>& categories)
    { enabled_categories = categories; }

    void enable_all()
    { enabled_categories = std::views::keys(debug_categories) | std::ranges::to<std::unordered_set>(); }

    void disable_all()
    { enabled_categories.clear(); }

    bool is_enabled(const std::string& category) const
    { return enabled_categories.contains(category); }

    void set_colors(bool enable) { use_colors = enable; }
    bool are_colors_enabled() const { return use_colors; }
    
    // Formatted debug output
    template<typename... Args>
    void log(const std::string& category,
        const std::string& format_str, Args&&... args)
    {
        if (!is_enabled(category)) return;
        
        std::string prefix = get_prefix(category);
        if (use_colors) {
            prefix = get_color(category) + prefix + "\033[0m";
        }
        
        // Use std::vformat instead of std::format with runtime format string
        std::string message = std::vformat(format_str, std::make_format_args(args...));
        std::println("{} {}", prefix, message);
    }
    
private:
    // The parameter cannot (yet) be a string_view because unordered_map
    // doesn't (yet) support heterogeneous lookups.
    std::string get_prefix(const std::string& category) const
    {
        if (not debug_categories.contains(category)) {
            throw std::runtime_error("Unknown debug category: " + category);
        }
        //TODO: Do we want to make this uppercase?
        return std::format("[{}]", category);
    }

    std::string get_color(const std::string& category) const
    {
        return debug_categories.at(category);
    }
};

debug_controller& get_debug();

#define VAU_DEBUG(category, ...) get_debug().log(#category, __VA_ARGS__)
#define VAU_DEBUG_ENABLED(category) get_debug().is_enabled(#category)

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

// String conversion functions
std::string to_string(int value);
std::string to_string(const std::string& value);
std::string to_string(const env_ptr& env);
std::string to_string(std::nullptr_t);
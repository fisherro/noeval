// Interpreter for a language built from fexprs and vau
// Code style will use snake_case and other conventions of the standard library

// NOTE THAT nil IS SPELT ()

#include <algorithm>
#include <cstdlib>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <memory>
#include <print>
#include <ranges>
#include <set>
#include <stacktrace>
#include <string_view>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <unistd.h>

#include "debug.hpp"
#include "noeval.hpp"
#include "parser.hpp"
#include "repl.hpp"
#include "tests.hpp"
#include "unicode.hpp"
#include "utils.hpp"

#define USE_TAIL_CALL 1

namespace {
    using cpp_int = boost::multiprecision::cpp_int;

    constexpr std::string_view digit_chars{"0123456789abcdefghijklmnopqrstuvwxyz"};

    // The digits of a non-negative integer in radix
    std::string integer_digits(cpp_int n, unsigned radix)
    {
        if (10 == radix) return n.str();
        if (0 == n) return "0";
        std::string digits;
        while (n > 0) {
            digits += digit_chars[(n % radix).convert_to<unsigned>()];
            n /= radix;
        }
        std::ranges::reverse(digits);
        return digits;
    }

    // Whether a fraction with this denominator (in lowest terms) has a
    // terminating expansion in radix: whether every prime factor of the
    // denominator divides radix.
    bool terminates(cpp_int denominator, unsigned radix)
    {
        for (cpp_int g = gcd(denominator, cpp_int{radix}); g > 1;
             g = gcd(denominator, cpp_int{radix})) {
            denominator /= g;
        }
        return 1 == denominator;
    }

    // The digits after the point of numerator/denominator, where numerator is
    // less than denominator, with any repeating digits in parentheses.
    std::string fraction_digits(cpp_int numerator, const cpp_int& denominator, unsigned radix)
    {
        // Where each remainder was first seen, to find where the digits repeat
        std::unordered_map<cpp_int, size_t> remainder_positions;
        std::string digits;
        while (0 != numerator) {
            if (auto seen = remainder_positions.find(numerator); seen != remainder_positions.end()) {
                return digits.substr(0, seen->second) + "(" + digits.substr(seen->second) + ")";
            }
            remainder_positions[numerator] = digits.length();
            numerator *= radix;
            digits += digit_chars[(numerator / denominator).convert_to<unsigned>()];
            numerator %= denominator;
        }
        return digits;
    }
}

std::string format_number(const bignum& value, number_style style, unsigned radix)
{
    if (radix < 2 or radix > 36) {
        throw std::invalid_argument(std::format("radix must be from 2 to 36, got {}", radix));
    }
    cpp_int numerator = boost::multiprecision::numerator(value);
    cpp_int denominator = boost::multiprecision::denominator(value);
    std::string result = (numerator < 0)? "-": "";
    numerator = abs(numerator);

    if (1 == denominator) {
        return result + integer_digits(numerator, radix);
    }
    if (number_style::fraction == style
        or (number_style::automatic == style and not terminates(denominator, radix))) {
        return result + integer_digits(numerator, radix) + "/" + integer_digits(denominator, radix);
    }
    return result + integer_digits(numerator / denominator, radix) + "."
        + fraction_digits(numerator % denominator, denominator, radix);
}

std::optional<bignum> parse_number(std::string_view text, unsigned radix)
{
    if (radix < 2 or radix > 36) {
        throw std::invalid_argument(std::format("radix must be from 2 to 36, got {}", radix));
    }

    // Read the digits at the start of text into value, and return how many
    // there were.
    auto read_digits = [&text, radix](cpp_int& value) {
        size_t count = 0;
        value = 0;
        while (not text.empty()) {
            char c = text.front();
            unsigned digit = ('0' <= c and c <= '9')? c - '0':
                             ('a' <= c and c <= 'z')? c - 'a' + 10:
                             ('A' <= c and c <= 'Z')? c - 'A' + 10: radix;
            if (digit >= radix) break;
            value = value * radix + digit;
            text.remove_prefix(1);
            ++count;
        }
        return count;
    };

    bool negative = text.starts_with('-');
    if (negative) text.remove_prefix(1);

    cpp_int whole;
    if (0 == read_digits(whole)) return std::nullopt;
    bignum result{whole};

    if (text.starts_with('/')) {
        text.remove_prefix(1);
        cpp_int denominator;
        if (0 == read_digits(denominator) or 0 == denominator) return std::nullopt;
        result = bignum(whole, denominator);
    } else if (text.starts_with('.')) {
        // x.y(z) is x + y/radix^|y| + z/(radix^|y| * (radix^|z| - 1))
        text.remove_prefix(1);
        cpp_int fixed;
        auto fixed_length = read_digits(fixed);
        cpp_int repeating;
        size_t repeating_length = 0;
        if (text.starts_with('(')) {
            text.remove_prefix(1);
            repeating_length = read_digits(repeating);
            if (0 == repeating_length or not text.starts_with(')')) return std::nullopt;
            text.remove_prefix(1);
        }
        if (0 == fixed_length and 0 == repeating_length) return std::nullopt;
        cpp_int scale = boost::multiprecision::pow(cpp_int{radix}, fixed_length);
        result += bignum(fixed, scale);
        if (0 != repeating_length) {
            result += bignum(repeating,
                scale * (boost::multiprecision::pow(cpp_int{radix}, repeating_length) - 1));
        }
    }

    if (not text.empty()) return std::nullopt;
    return negative? -result: result;
}

std::string to_string(const bignum& value)
{
    return format_number(value);
}

std::string to_string(const std::string& value)
{
    std::string result = "\"";
    for (char c: value) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    result += "\"";
    return result;
}

std::string to_string(const env_ptr& env)
{ 
    return std::format("#<environment:{}>",
        static_cast<const void*>(env.get())); 
}

std::string to_string(std::nullptr_t) { return "()"; }

// Helper function to print values for debugging
std::string value_to_string(const value_ptr& val)
{
    return std::visit([=](const auto& v) -> std::string {
        // Check if the type has a member to_string() function
        if constexpr (requires { v.to_string(); }) {
            return v.to_string();
        }
        // Otherwise use free function to_string()
        else {
            return to_string(v);
        }
    }, val->data);
}

std::string value_to_string(const continuation_type& k)
{
    struct visitor {
        std::string operator()(const tail_call& tc) const
        {
            return std::format("(tail-call {} {})",
                value_to_string(tc.expr),
                value_to_string(value::make(tc.env)));
        }
        std::string operator()(const value_ptr& v) const
        {
            return value_to_string(v);
        }
    };
    return std::visit(visitor{}, k);
}

std::string value_type_string(const value_ptr& val)
{
    return std::visit([](const auto& v) -> std::string {
        return demangle<decltype(v)>();
    }, val->data);
}

std::string source_location::to_string() const
{
    if (not file) return "";
    return std::format("{}:{}:{}", *file, line, column);
}

const std::string* intern_file_name(std::string_view name)
{
    // Set nodes don't move, so the pointers stay valid.
    static std::set<std::string, std::less<>> names;
    auto it = names.find(name);
    if (names.end() == it) it = names.emplace(name).first;
    return &*it;
}

std::string cons_cell::to_string() const
{
    // Reconstruct the value_ptr for this cons_cell to reuse value_to_string
    // Double ownership!? No, this makes a copy.
    auto this_val = value::make(*this);
    
    std::string result = "(";
    auto current = this_val;
    bool first = true;
    while (std::holds_alternative<cons_cell>(current->data)) {
        if (not first) result += " ";
        first = false;
        auto& cell = std::get<cons_cell>(current->data);
        result += value_to_string(cell.car);
        current = cell.cdr;
    }
    if (not std::holds_alternative<std::nullptr_t>(current->data)) {
        result += " . " + value_to_string(current);
    }
    result += ")";
    return result;
}

bool cons_cell::operator==(const cons_cell& that) const
{
    return *car == *(that.car) and *cdr == *(that.cdr);
}

std::string operative::to_string() const
{
    if (not tag.empty()) return tag;
    // It isn't easy (yet) to change the delimiter that format uses for ranges,
    // so explicitly use std::views::join_with. (Converting to a string, rather
    // than formatting the range directly, also supports GCC 14.)
    return std::format("(#<operative> {}{}{} {} {})",
        params.is_variadic? "": "(",
        params.param_names | std::views::join_with(' ') | std::ranges::to<std::string>(),
        params.is_variadic? "": ")",
        env_param,
        value_to_string(body)
    );
}

std::string macro::to_string() const
{ return "#<macro:" + value_to_string(transformer) + ">"; }

std::string mutable_binding::to_string() const
{ return "#<mutable:" + value_to_string(value) + ">"; }


// Helper function to add context to expressions
std::string expr_context(const value_ptr& expr)
{
    try {
        return value_to_string(expr);
    } catch (...) {
        return "<expression>";
    }
}

// Nested mutable bindings would be ugly. Can define-mutable even created them?
value_ptr unwrap_mutable_binding(value_ptr value)
{
    if (auto mb{std::get_if<mutable_binding>(&value->data)}; mb) {
        return mb->value;
    }
    return value;
}

// Should be const references...
bool operator==(value& lhs, value& rhs)
{
    value_ptr lhs_ptr = lhs.shared_from_this();
    value_ptr rhs_ptr = rhs.shared_from_this();
    value_ptr lhs_unwrapped = unwrap_mutable_binding(lhs_ptr);
    value_ptr rhs_unwrapped = unwrap_mutable_binding(rhs_ptr);

    auto lhs_nil{is_nil(lhs_unwrapped)};
    auto rhs_nil{is_nil(rhs_unwrapped)};
    if (lhs_nil and rhs_nil) return true;
    if (lhs_nil or rhs_nil) return false;

    if (lhs_unwrapped->data.index() != rhs_unwrapped->data.index()) {
        throw std::runtime_error(
            std::format(
                "Incompatible value types: {} vs {}",
                value_type_string(lhs_unwrapped),
                value_type_string(rhs_unwrapped)));
    }

    return lhs_unwrapped->data == rhs_unwrapped->data;
}

std::string typeof_visitor::operator()(const mutable_binding& mb) const
{
    return std::visit(typeof_visitor{}, mb.value->data);
}

// Environment implementation
value_ptr environment::lookup(const std::string& name) const
{
    NOEVAL_DEBUG(env_lookup, "Looking up '{}' in env {}", name, static_cast<const void*>(this));
    
    if (NOEVAL_DEBUG_ENABLED(env_dump)) {
        NOEVAL_DEBUG(env_dump, "Current bindings:");
        for (const auto& [key, value]: bindings) {
            NOEVAL_DEBUG(env_dump, "  {} -> {}", key, value_to_string(value));
        }
        if (parent) {
            NOEVAL_DEBUG(env_dump, "Parent env: {}", static_cast<const void*>(parent.get()));
        }
    }

    auto it = bindings.find(name);
    if (it != bindings.end()) {
        NOEVAL_DEBUG(env_lookup, "Found '{}' in current environment", name);
        return it->second;
    }
    if (parent) {
        NOEVAL_DEBUG(env_lookup, "Not found, checking parent...");
        return parent->lookup(name);
    }
    throw std::runtime_error("Unbound variable: " + name);
}

void environment::define(const std::string& name, value_ptr val)
{
    NOEVAL_DEBUG(env_binding, "Binding '{}' in env {} to {}", 
              name, static_cast<const void*>(this), value_to_string(val));
    bindings[name] = std::move(val);
}

std::vector<std::string> environment::get_all_symbols() const
{
    auto symbols = bindings | std::views::keys | std::ranges::to<std::vector>();
    if (parent) {
        std::ranges::copy(parent->get_all_symbols(), std::back_inserter(symbols));
    }
    return symbols;
}

std::vector<std::string> environment::get_own_symbols() const
{
    auto symbols = bindings | std::views::keys | std::ranges::to<std::vector>();
    std::ranges::sort(symbols);
    return symbols;
}

std::string environment::dump_chain() const
{
    std::string chain = std::format("{}", static_cast<const void*>(this));
    if (parent) {
        chain += " -> " + parent->dump_chain();
    }
    return chain;
}

// Check if a value is nil
bool is_nil(const value_ptr& val)
{
    return std::holds_alternative<std::nullptr_t>(val->data);
}

// Check if a value is a cons cell
bool is_cons(const value_ptr& val)
{
    return std::holds_alternative<cons_cell>(val->data);
}

// Get car of a cons cell
value_ptr car(const value_ptr& val)
{
    if (not is_cons(val)) {
        throw std::runtime_error("car: not a cons cell");
    }
    return std::get<cons_cell>(val->data).car;
}

// Get cdr of a cons cell
value_ptr cdr(const value_ptr& val)
{
    if (not is_cons(val)) {
        throw std::runtime_error("cdr: not a cons cell");
    }
    return std::get<cons_cell>(val->data).cdr;
}

// Convert a list to a vector for easier processing
std::vector<value_ptr> list_to_vector(value_ptr list)
{
    std::vector<value_ptr> result;
    auto current = list;
    while (is_cons(current)) {
        result.push_back(car(current));
        current = cdr(current);
    }
    if (not is_nil(current)) {
        throw std::runtime_error("Improper list");
    }
    return result;
}

// Helper to construct a Noeval list from C++ values
value_ptr make_list(std::initializer_list<value_ptr> elements)
{
    value_ptr result = value::make(nullptr); // Start with nil
    
    // Build list backwards
    for (auto it = elements.end(); it != elements.begin(); ) {
        --it;
        result = value::make(cons_cell{*it, result});
    }
    
    return result;
}

value_ptr quote(value_ptr expr)
{
    return make_list({
        value::make(symbol{"q"}),
        expr,
    });
}

// Extract parameter list from a list value
param_pattern extract_param_pattern(value_ptr params)
{
    // Handle single symbol case: (vau args env ...)
    if (std::holds_alternative<symbol>(params->data)) {
        return {true, {std::get<symbol>(params->data).name}};
    }

    // Handle list cases: (vau (a b . rest) env ...) or (vau (a b) env ...)
    std::vector<std::string> fixed;
    auto current = params;
    
    while (is_cons(current)) {
        auto param = car(current);
        if (not std::holds_alternative<symbol>(param->data)) {
            throw std::runtime_error("Parameter must be a symbol");
        }
        fixed.push_back(std::get<symbol>(param->data).name);
        current = cdr(current);
    }
    
    if (is_nil(current)) {
        // Proper list: (a b c)
        return { false, fixed };
    } else {
        throw std::runtime_error("Invalid parameter pattern");
    }
}

// Forward declarations:
continuation_type operate_operative(const operative& op, value_ptr operands, env_ptr env);
continuation_type operate_builtin(const builtin_operative& op, value_ptr operands, env_ptr env);

struct call_stack {
private:
    // We store the expressions rather than their string forms. Converting
    // every expression to a string as it is evaluated is expensive, and we
    // only need the strings when formatting a stack trace. (Expressions are
    // immutable, so the strings will be the same.)
    struct frame {
        // The expression eval was called with
        value_ptr expr;
        // A tail call reuses the frame, which would otherwise hide where
        // evaluation had got to. So this is the latest expression reached by
        // tail call that has a source location, or null if there isn't one.
        value_ptr tail_expr;
    };

    // These were thread_local, but we aren't using threads (yet).
    inline static std::vector<frame> stack;
    inline static size_t max_depth{0};

    static source_location location_of(const value_ptr& expr)
    {
        if (not expr) return {};
        if (auto cell = std::get_if<cons_cell>(&expr->data)) {
            return cell->location;
        }
        return {};
    }

    static std::string describe(const value_ptr& expr)
    {
        if (auto loc = location_of(expr)) {
            return std::format("{} at {}", value_to_string(expr), loc.to_string());
        }
        return value_to_string(expr);
    }

public:
    struct guard {
        guard(value_ptr expr)
        {
            stack.push_back({std::move(expr), nullptr});
            if (depth() > max_depth) max_depth = depth();
        }
        ~guard()
        {
            if (not stack.empty()) {
                stack.pop_back();
            }
        }
        // Record a tail call. Any frames pushed since this guard's have been
        // popped, so its frame is the top one.
        void tail_call(const value_ptr& expr)
        {
            if (location_of(expr)) stack.back().tail_expr = expr;
        }
    };

    static std::string format()
    {
        std::string result;
        for (const auto& [index, f]: stack | std::views::enumerate) {
            result += std::format("{}: {}\n", index, describe(f.expr));
            if (f.tail_expr) {
                result += std::format("   tail call: {}\n", describe(f.tail_expr));
            }
        }
        return result;
    }

    // The location of the innermost frame that has one
    static source_location location()
    {
        for (const auto& f: stack | std::views::reverse) {
            if (auto loc = location_of(f.tail_expr)) return loc;
            if (auto loc = location_of(f.expr)) return loc;
        }
        return {};
    }

    static size_t depth() { return stack.size(); }
    static std::string indent() { return std::string(depth() * 2, ' '); }

    static void  reset_max_depth() { max_depth = 0; }
    static size_t get_max_depth()  { return max_depth; }
};

std::string current_source_location() { return call_stack::location().to_string(); }

void   call_stack_reset_max_depth() { call_stack::reset_max_depth(); }
size_t call_stack_get_max_depth()   { return call_stack::get_max_depth(); }

// Built-in operatives
namespace builtins {

    // The call an operative was given, "(name operand ...)", for error messages
    std::string call_context(std::string_view name, const std::vector<value_ptr>& args)
    {
        auto result = std::format("({}", name);
        for (const auto& arg: args) {
            result += " " + expr_context(arg);
        }
        return result + ")";
    }

    // Throw unless an operative was given exactly count operands. params, if
    // given, describes them for the error message, e.g. "(symbol value)".
    void expect_args(std::string_view name, const std::vector<value_ptr>& args,
        size_t count, std::string_view params = "")
    {
        if (count == args.size()) return;
        throw evaluation_error(
            std::format("{}: expected {} argument{}{}{}, got {}",
                name, count, (1 == count)? "": "s",
                params.empty()? "": " ", params, args.size()),
            call_context(name, args),
            call_stack::format()
        );
    }

    continuation_type vau_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("vau", args, 3, "(params env-param body)");
        
        auto params_expr = args[0];
        auto env_param_expr = args[1];
        auto body_expr = args[2];
        
        try {
            // Extract parameter pattern
            auto param_pattern = extract_param_pattern(params_expr);
            
            std::string env_param_name;
            // We use nil as the equivalent to Kernel's #ignore for vau's
            // environment parameter.
            if (not is_nil(env_param_expr)) {
                // Extract environment parameter name
                if (not std::holds_alternative<symbol>(env_param_expr->data)) {
                    throw evaluation_error(
                        "vau: environment parameter must be a symbol",
                        std::format("(vau {} {} {})", expr_context(params_expr), 
                                expr_context(env_param_expr), expr_context(body_expr)),
                        call_stack::format()
                    );
                }
                env_param_name = std::get<symbol>(env_param_expr->data).name;
            }
            
            // Create the operative
            return value::make(operative{
                std::move(param_pattern),
                std::move(env_param_name),
                body_expr,
                env,
            });
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("vau: {}", e.what()),
                std::format("(vau {} {} {})", expr_context(params_expr), 
                           expr_context(env_param_expr), expr_context(body_expr)),
                call_stack::format()
            );
        }
    }

    // Helper function to validate eval arguments
    void validate_eval_arguments(const std::vector<value_ptr>& args)
    {
        expect_args("eval", args, 2, "(expr env)");
    }

    // Helper function to evaluate both arguments in current environment
    std::pair<value_ptr, value_ptr> evaluate_eval_arguments(const std::vector<value_ptr>& args, env_ptr env)
    {
        auto expr = args[0];          // Expression to evaluate (unevaluated)
        auto env_expr = args[1];      // Environment expression (unevaluated)

        NOEVAL_DEBUG(operative, "eval_operative called in environment {}", to_string(env));
        NOEVAL_DEBUG(operative, "First argument (expr): {}", expr_context(expr));
        NOEVAL_DEBUG(operative, "Second argument (env_expr): {}", expr_context(env_expr));

        // STAGE 1: Evaluate BOTH arguments in the CURRENT environment
        auto evaluated_expr = eval(expr, env);  // This is the key change!
        NOEVAL_DEBUG(operative, "First argument evaluated to: {}", value_to_string(evaluated_expr));

        auto env_val = eval(env_expr, env);
        NOEVAL_DEBUG(operative, "Environment expression evaluated to: {}", value_to_string(env_val));

        return {evaluated_expr, env_val};
    }

    // Helper function to extract environment from evaluated value
    env_ptr extract_target_environment(value_ptr env_val, const std::vector<value_ptr>& args)
    {
        if (not std::holds_alternative<env_ptr>(env_val->data)) {
            throw evaluation_error(
                std::format("eval: second argument must evaluate to an environment, got {}",
                        value_to_string(env_val)),
                std::format("(eval {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        
        auto target_env = std::get<env_ptr>(env_val->data);
        NOEVAL_DEBUG(operative, "Target environment for evaluation: {}", to_string(target_env));

        return target_env;
    }

    // Evaluates both arguments, then evaluates the result of evaluating the
    // first argument in the environment evaluated from the second argument.
    continuation_type eval_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        try {
            validate_eval_arguments(args);
            
            auto [evaluated_expr, env_val] = evaluate_eval_arguments(args, env);
            
            auto target_env = extract_target_environment(env_val, args);
            
            NOEVAL_DEBUG(operative, "About to evaluate {} in target environment", 
                    value_to_string(evaluated_expr));
#ifdef USE_TAIL_CALL
            return tail_call{evaluated_expr, target_env};
#else
            return eval(evaluated_expr, target_env);
#endif
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("eval: {}", e.what()),
                std::format("(eval {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
    }

    // Does not evaluate first argument, but evaluates the second
    // define and define-mutable can't rebind a name in the same environment.
    // (A binding in an ancestor environment can be shadowed.)
    void check_not_bound(std::string_view op_name, const env_ptr& env, const std::string& name)
    {
        if (env->binds(name)) {
            throw evaluation_error(
                std::format("{}: {} is already defined in this environment "
                            "(use set! to change a mutable binding, or let for a new scope)",
                            op_name, name),
                std::string{op_name},
                call_stack::format()
            );
        }
    }

    // define, and the REPL's redefine, which may rebind a name
    continuation_type define_binding(std::string_view op_name, bool allow_rebind,
        const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args(op_name, args, 2, "(symbol value)");
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (not std::holds_alternative<symbol>(sym_expr->data)) {
            throw evaluation_error(
                std::format("{}: first argument must be a symbol, got {}", op_name, expr_context(sym_expr)),
                std::format("({} {} {})", op_name, expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
        
        auto sym_name = std::get<symbol>(sym_expr->data).name;
        if (not allow_rebind) check_not_bound(op_name, env, sym_name);

        try {
            auto val = eval(val_expr, env);
            
            env->define(sym_name, val);
            return val;
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("{}: {}", op_name, e.what()),
                std::format("({} {} {})", op_name, expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
    }

    continuation_type define_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        return define_binding("define", false, args, env);
    }

    continuation_type redefine_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        return define_binding("redefine", true, args, env);
    }

    // Helper function to validate and extract number from value
    bignum extract_number(const value_ptr& val, const std::string& op_name, const value_ptr& original_arg)
    {
        if (not std::holds_alternative<bignum>(val->data)) {
            throw evaluation_error(
                std::format("{}: argument must be a number, got {}", 
                           op_name, value_to_string(val)),
                std::format("... {} ...", expr_context(original_arg)),
                call_stack::format()
            );
        }
        return std::get<bignum>(val->data);
    }

    // Helper function to evaluate and validate the first argument
    bignum evaluate_first_argument(const value_ptr& first_arg, const std::string& op_name, env_ptr env)
    {
        auto first_val = eval(first_arg, env);
        if (not std::holds_alternative<bignum>(first_val->data)) {
            throw evaluation_error(
                std::format("{}: argument must be a number, got {}", 
                           op_name, value_to_string(first_val)),
                std::format("({} {} ...)", op_name, expr_context(first_arg)),
                call_stack::format()
            );
        }
        return std::get<bignum>(first_val->data);
    }

    // Helper function to build error context for arithmetic operations
    std::string build_arithmetic_context(const std::string& op_name, const std::vector<value_ptr>& args)
    {
        std::string context = "(" + op_name;
        for (const auto& arg: args) {
            context += " " + expr_context(arg);
        }
        context += ")";
        return context;
    }

    // Arithmetic follows Scheme. With no arguments, + and * return their
    // identity (0 and 1), and - and / raise an error. With one argument, the
    // result is (op identity x), so (- x) negates and (/ x) is the reciprocal.
    // Otherwise, op is folded over the arguments from the left.
    auto make_arithmetic_operative(const std::string& op_name,
        std::function<bignum(bignum, bignum)> op, bignum identity,
        bool identity_without_arguments)
    {
        return [op_name, op, identity, identity_without_arguments](
            const std::vector<value_ptr>& args, env_ptr env)
        {
            if (args.empty()) {
                if (identity_without_arguments) return value::make(identity);
                throw evaluation_error(
                    std::format("{}: requires at least one argument", op_name),
                    std::format("({})", op_name),
                    call_stack::format()
                );
            }

            try {
                bignum initial_value = evaluate_first_argument(args[0], op_name, env);
                if (1 == args.size()) {
                    return value::make(op(identity, initial_value));
                }

                bignum result = std::ranges::fold_left(args | std::views::drop(1),
                    initial_value,
                    [op, op_name, &env](bignum accumulator, const value_ptr& arg)
                    {
                        auto val = eval(arg, env);
                        bignum operand = extract_number(val, op_name, arg);
                        return op(accumulator, operand);
                    });
                    
                return value::make(result);
            } catch (const evaluation_error&) {
                throw; // Re-throw evaluation errors as-is
            } catch (const std::exception& e) {
                throw evaluation_error(
                    std::format("{}: {}", op_name, e.what()),
                    build_arithmetic_context(op_name, args),
                    call_stack::format()
                );
            }
        };
    }

    // Evaluates both arguments
    continuation_type cons_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("cons", args, 2, "(first rest)");
        
        auto first_val = eval(args[0], env);
        auto rest_val = eval(args[1], env);
        
        return value::make(cons_cell{first_val, rest_val});
    }

    // Evaluates argument
    continuation_type first_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("first", args, 1);
        
        auto val = eval(args[0], env);
        return car(val);  // Uses existing helper
    }

    // Evaluates argument
    continuation_type rest_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("rest", args, 1);
        
        auto val = eval(args[0], env);
        return cdr(val);  // Uses existing helper
    }

    // Helper to build `(eval symbol env)`
    value_ptr make_eval_expression(std::string_view symbol_name)
    {
        return value::make(cons_cell{
            value::make(symbol{"eval"}),
            value::make(cons_cell{
                value::make(symbol{symbol_name}),
                value::make(cons_cell{
                    value::make(symbol{"env"}),
                    value::make(nullptr),  // nil
                }),
            }),
        });
    }

    auto church_true(env_ptr env)
    {
        //Lookup true in the given environment.
        return env->lookup("true");
    }

    auto church_false(env_ptr env)
    {
        //Lookup false in the given environment.
        return env->lookup("false");
    }

    // Evaluates argument
    // Returns Church Booleans
    continuation_type nil_p_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("nil?", args, 1);
        
        auto val = eval(args[0], env);
        return is_nil(val)? church_true(env): church_false(env);
    }

    continuation_type invoke_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("invoke", args, 2, "(operative arg-list)");

        auto op_expr = args[0];  // The operative to invoke (unevaluated)
        auto arg_list = eval(args[1], env);  // The list of arguments
        
        // Convert the argument list to a vector
        auto arg_vector = list_to_vector(arg_list);
        
        // Create a new expression: (operative arg1 arg2 ...)
        auto call_expr = value::make(cons_cell{op_expr, arg_list});
        
        return eval(call_expr, env);
    }

    // Evaluates both arguments, then evaluates each element of the list in the
    // environment and returns a list of the results.
    // This used to be in the library, but its helper operative created a
    // cycle (environment → binding → operative → closure environment) on every
    // call, and nearly every call to a wrapped operative goes through it.
    continuation_type eval_list_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("eval-list", args, 2, "(list env)");

        auto env_val = eval(args[1], env);
        auto list = eval(args[0], env);
        if (not is_nil(list) and not is_cons(list)) {
            throw evaluation_error(
                "eval-list's first argument must be a list",
                std::format("(eval-list {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        if (not std::holds_alternative<env_ptr>(env_val->data)) {
            throw evaluation_error(
                std::format("eval-list: second argument must evaluate to an environment, got {}",
                        value_to_string(env_val)),
                std::format("(eval-list {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        auto target_env = std::get<env_ptr>(env_val->data);

        std::vector<value_ptr> results;
        for (const auto& expr: list_to_vector(list)) {
            results.push_back(eval(expr, target_env));
        }

        value_ptr result = value::make(nullptr);
        for (auto it = results.rbegin(); it != results.rend(); ++it) {
            result = value::make(cons_cell{*it, result});
        }
        return result;
    }

    // Evaluates each argument
    continuation_type do_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.empty()) {
            // Empty do returns nil
            return value::make(nullptr);
        }
        
        try {
            value_ptr result = value::make(nullptr); // default to nil
            
            // Evaluate each expression in sequence, keeping the last result
            for (const auto& expr: std::ranges::subrange{args.begin(), args.end() - 1}) {
                result = eval(expr, env);
            }

#ifdef USE_TAIL_CALL
            return tail_call{args.back(), env};
#else
            return eval(args.back(), env);
#endif
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            // Build context showing all expressions
            std::string context = "(do";
            for (const auto& arg: args) {
                context += " " + expr_context(arg);
            }
            context += ")";
            
            throw evaluation_error(
                std::format("do: {}", e.what()),
                context,
                call_stack::format()
            );
        }
    }

    // This implements structural equality
    // It evaluates its arguments.
    // Anything compared against () is false except () itself.
    // Mutable bindings are compared as if they were their underlying value.
    // In other cases, comparison against different types raises an error.
    // Comparison between true and true or false and false returns true.
    // All other comparisons between operatives always return false.
    continuation_type equal_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("=", args, 2);

        auto val1 = eval(args[0], env);
        auto val2 = eval(args[1], env);

        return (*val1 == *val2)? church_true(env): church_false(env);
    }

    continuation_type write_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("write", args, 1);
        
        try {
            auto val = eval(args[0], env);
            std::print("{}", value_to_string(val));
            return val;  // Return the value that was written
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("write: {}", e.what()),
                std::format("(write {})", expr_context(args[0])),
                call_stack::format()
            );
        }
    }

    continuation_type display_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("display", args, 1);
        
        try {
            auto val = eval(args[0], env);
            
            // Handle strings specially - output without quotes and interpret escapes
            if (std::holds_alternative<std::string>(val->data)) {
                std::print("{}", std::get<std::string>(val->data));
            } else {
                // For non-strings, use the same as write
                std::print("{}", value_to_string(val));
            }
            
            return val;
        } catch (const evaluation_error&) {
            throw;
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("display: {}", e.what()),
                std::format("(display {})", expr_context(args[0])),
                call_stack::format()
            );
        }
    }

    continuation_type flush_operative(const std::vector<value_ptr>& args, env_ptr)
    {
        expect_args("flush", args, 0);
        
        // Flush the standard output
        std::fflush(stdout);
        return value::make(nullptr);  // Return nil
    }

    continuation_type define_mutable_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("define-mutable", args, 2, "(symbol value)");
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (not std::holds_alternative<symbol>(sym_expr->data)) {
            throw evaluation_error(
                "define-mutable: first argument must be a symbol",
                std::format("(define-mutable {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
        
        auto sym_name = std::get<symbol>(sym_expr->data).name;
        check_not_bound("define-mutable", env, sym_name);

        try {
            auto val = eval(val_expr, env);
            
            // Wrap the value in a mutable_binding
            auto mutable_val = value::make(mutable_binding{val});
            env->define(sym_name, mutable_val);
            return val;  // Return the original value, not the wrapper
        } catch (const evaluation_error&) {
            throw;
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("define-mutable: {}", e.what()),
                std::format("(define-mutable {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
    }

    continuation_type set_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("set!", args, 2, "(symbol value)");
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (not std::holds_alternative<symbol>(sym_expr->data)) {
            throw evaluation_error(
                "set!: first argument must be a symbol",
                std::format("(set! {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
        
        try {
            auto sym_name = std::get<symbol>(sym_expr->data).name;
            auto new_value = eval(val_expr, env);
            
            // Look up the current binding
            auto current_binding = env->lookup(sym_name);
            
            // Check if it's mutable
            if (not std::holds_alternative<mutable_binding>(current_binding->data)) {
                throw evaluation_error(
                    std::format("set!: variable '{}' is not mutable (use define-mutable)", sym_name),
                    std::format("(set! {} {})", expr_context(sym_expr), expr_context(val_expr)),
                    call_stack::format()
                );
            }
            
            // Update the mutable binding
            std::get<mutable_binding>(current_binding->data).value = new_value;
            return new_value;
            
        } catch (const evaluation_error&) {
            throw;
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("set!: {}", e.what()),
                std::format("(set! {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
    }

    // Either: (try try-expr handler-operative)
    //     or: (try try-expr handler-operative finally-operative)
    //
    // The handler-operative must have one parameter: The error.
    // The finally-operative must have one parameter: The try-expr result.
    //
    // I'm not convinced finally is particularly useful in this language.
    // But we'll see.
    //
    // This is a stop-gap measure. I plan to revisit error handling in the
    // future, but I need something quick and dirty for now.
    continuation_type try_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if ((args.size() < 2) or (args.size() > 3)) {
            throw evaluation_error(
                std::format("try: expected 2 arguments (expr handler) or 3 (expr handler finally), got {}", args.size()),
                "try",
                call_stack::format()
            );
        }

        auto try_expr = args[0];
        auto handler_expr = args[1];
        value_ptr error_val;
        value_ptr result;

        try {
            result = eval(try_expr, env);
        } catch (const evaluation_error& e) {
            error_val = make_list({
                value::make(symbol{"error"}),
                value::make(e.message),
                value::make(e.context),
                value::make(e.stack_trace),
            });
        } catch (const std::exception& e) {
            error_val = make_list({
                value::make(symbol{"error"}),
                value::make(e.what()),
                value::make(std::string{}),  // context
                value::make(std::string{}),  // stack trace
            });
        } catch (...) {
            error_val = make_list({
                value::make(symbol{"error"}),
                value::make(std::string{"unknown error"}),
                value::make(std::string{}),  // context
                value::make(std::string{}),  // stack trace
            });
        }

        if (error_val) {
            // If we got to here, an exception was caught.
            // We deal with it here instead of in the catch blocks to avoid a
            // termination caused by throwing an exception while handling an
            // exception.

            // Evaluate handler with error as argument
            auto handler_call = make_list({
                handler_expr,
                quote(error_val),
            });
            result = eval(handler_call, env);
        }

        if (3 == args.size()) {
            auto finally_thunk = args[2];
            auto finally_call = make_list({ finally_thunk, quote(result) });
            result = eval(finally_call, env);
        }

        return result;
    }

    continuation_type raise_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("raise", args, 1, "(error-message)");
        
        auto message_val = eval(args[0], env);
        
        std::string message;
        if (std::holds_alternative<std::string>(message_val->data)) {
            message = std::get<std::string>(message_val->data);
        } else {
            message = value_to_string(message_val);
        }
        
        throw evaluation_error(message, "", call_stack::format());
    }

    // Evaluates its argument, which must be an operative, and returns a macro
    // with it as the transformer.
    continuation_type macro_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("macro", args, 1);
        auto transformer = unwrap_mutable_binding(eval(args[0], env));
        if (not (std::holds_alternative<operative>(transformer->data) or
                 std::holds_alternative<builtin_operative>(transformer->data))) {
            throw evaluation_error(
                std::format("macro: argument must be an operative, got {}",
                    value_to_string(transformer)),
                call_context("macro", args),
                call_stack::format()
            );
        }
        return value::make(macro{transformer});
    }

    continuation_type typeof_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("typeof", args, 1);
        auto arg = eval(args[0], env);
        std::string type = std::visit(typeof_visitor{}, arg->data);
        return value::make(symbol{type});
    }

    continuation_type spaceship_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("<=>", args, 2);

        auto left_unwrap  = unwrap_mutable_binding(eval(args[0], env));
        auto right_unwrap = unwrap_mutable_binding(eval(args[1], env));
        auto left  = std::get_if<bignum>(&(left_unwrap->data));
        auto right = std::get_if<bignum>(&(right_unwrap->data));

        if ((not left) or (not right)) {
            throw evaluation_error(
                "<=>: both arguments must be numbers",
                std::format("(<=> {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }

        int result{0};
        if (*left < *right) {
            result = -1;
        } else if (*left > *right) {
            result = 1;
        }
        return value::make(result);
    }

    continuation_type numerator_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("numerator", args, 1);
        auto val = eval(args[0], env);
        auto n = std::get_if<bignum>(&val->data);
        if (not n) {
            throw evaluation_error(
                std::format("numerator: argument must be a number, got {}", value_to_string(val)),
                "numerator",
                call_stack::format()
            );
        }
        bignum numerator = boost::multiprecision::numerator(*n);
        return value::make(numerator);
    }

    continuation_type denominator_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("denominator", args, 1);
        auto val = eval(args[0], env);
        auto n = std::get_if<bignum>(&val->data);
        if (not n) {
            throw evaluation_error(
                std::format("denominator: argument must be a number, got {}", value_to_string(val)),
                "denominator",
                call_stack::format()
            );
        }
        bignum denominator = boost::multiprecision::denominator(*n);
        return value::make(denominator);
    }

    continuation_type remainder_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("remainder", args, 2);
        
        auto val1 = eval(args[0], env);
        auto val2 = eval(args[1], env);
        
        auto n1 = std::get_if<bignum>(&val1->data);
        auto n2 = std::get_if<bignum>(&val2->data);
        if (not n1 or not n2) {
            throw evaluation_error(
                "remainder: both arguments must be numbers",
                "remainder",
                call_stack::format()
            );
        }
        
        // For rationals: a remainder b = a - truncate(a/b) * b
        bignum quotient = *n1 / *n2;
        
        // Truncate toward zero for rational numbers
        // Convert to integer directly (this truncates toward zero)
        using cpp_int = boost::multiprecision::cpp_int;
        cpp_int truncated_int = boost::multiprecision::numerator(quotient) / 
                            boost::multiprecision::denominator(quotient);
        
        // Convert back to bignum (rational)
        bignum truncated_quotient{truncated_int};
        
        bignum result = *n1 - truncated_quotient * *n2;
        return value::make(result);
    }

    continuation_type string_to_list_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("string->list", args, 1);
        
        auto str_val = eval(args[0], env);
        if (not std::holds_alternative<std::string>(str_val->data)) {
            throw evaluation_error(
                std::format("string->list: argument must be a string, got {}", value_to_string(str_val)),
                "string->list",
                call_stack::format()
            );
        }
        
        const auto& str = std::get<std::string>(str_val->data);
        // Inefficient conversion but safer than a reinterpret_cast:
        // (There's an argument that `value` should use std::u8string instead.)
        const std::u8string utf8(str.begin(), str.end());
        const auto utf32 = utf8_to_utf32(utf8);
        auto result = value::make(nullptr);
        
        // Build the list in reverse order
        for (auto it = utf32.rbegin(); it != utf32.rend(); ++it) {
            result = value::make(cons_cell{
                value::make(bignum{*it}), result});
        }
        
        return result;
    }

    char32_t bignum_to_char32(const bignum& rational)
    {
        namespace bmp = boost::multiprecision;
        if (1 != bmp::denominator(rational)) {
            throw std::invalid_argument("list->string: codepoint must be an integer");
        }
        bmp::cpp_int numerator = bmp::numerator(rational);
        if ((numerator < 0) or (numerator > 0x10FFFF)) {
            throw std::invalid_argument(
                std::format("list->string: Invalid Unicode codepoint {} (must be 0-0x10FFFF)",
                    numerator.str()));
        }
        char32_t codepoint = numerator.convert_to<char32_t>();
        if (codepoint >= 0xD800 and codepoint <= 0xDFFF) {
            throw std::invalid_argument(
                std::format("list->string: Invalid Unicode codepoint U+{:X} (surrogate pair range not allowed)",
                    static_cast<uint32_t>(codepoint)));
        }
        return codepoint;
    }

    continuation_type list_to_string_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("list->string", args, 1);
        
        auto list_val = eval(args[0], env);

        if (std::holds_alternative<nullptr_t>(list_val->data)) {
            return value::make(std::string{});
        }

        if (not std::holds_alternative<cons_cell>(list_val->data)) {
            throw evaluation_error(
                std::format("list->string: argument must be a list, got {}", value_to_string(list_val)),
                "list->string",
                call_stack::format()
            );
        }
        
        std::u32string result;
        auto current = list_val;
        
        // Traverse the list and convert each bignum to char32_t
        while (std::holds_alternative<cons_cell>(current->data)) {
            const auto& cell = std::get<cons_cell>(current->data);
            if (not std::holds_alternative<bignum>(cell.car->data)) {
                throw evaluation_error(
                    "list->string: all elements must be numbers",
                    "list->string",
                    call_stack::format()
                );
            }
            bignum bn = std::get<bignum>(cell.car->data);
            result.push_back(bignum_to_char32(bn));
            current = cell.cdr;
        }

        std::u8string utf8 = utf32_to_utf8(result);
        std::string s(utf8.begin(), utf8.end());

        return value::make(s);
    }

    // Evaluates its argument, which must be a string, and returns the symbol
    // with that name. Like Scheme's, it accepts any string, including ones
    // that wouldn't read back as that symbol, such as "" or "a b".
    continuation_type string_to_symbol_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("string->symbol", args, 1);
        auto str_val = eval(args[0], env);
        auto str = std::get_if<std::string>(&str_val->data);
        if (not str) {
            throw evaluation_error(
                std::format("string->symbol: argument must be a string, got {}", value_to_string(str_val)),
                "string->symbol",
                call_stack::format()
            );
        }
        return value::make(symbol{*str});
    }

    // Evaluates its argument, which must be a symbol, and returns its name as a
    // string.
    continuation_type symbol_to_string_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("symbol->string", args, 1);
        auto sym_val = eval(args[0], env);
        auto sym = std::get_if<symbol>(&sym_val->data);
        if (not sym) {
            throw evaluation_error(
                std::format("symbol->string: argument must be a symbol, got {}", value_to_string(sym_val)),
                "symbol->string",
                call_stack::format()
            );
        }
        return value::make(sym->name);
    }

    // Raise an error for name, with the call as its context
    [[noreturn]] void raise_argument_error(std::string_view name,
        const std::vector<value_ptr>& args, std::string message)
    {
        throw evaluation_error(std::format("{}: {}", name, message),
            call_context(name, args), call_stack::format());
    }

    // A radix argument's value: an integer from 2 to 36
    unsigned radix_argument(std::string_view name, const std::vector<value_ptr>& args,
        const value_ptr& arg)
    {
        auto n = std::get_if<bignum>(&arg->data);
        if (not n or 1 != denominator(*n) or *n < 2 or *n > 36) {
            raise_argument_error(name, args, std::format(
                "radix must be an integer from 2 to 36, got {}", value_to_string(arg)));
        }
        return numerator(*n).convert_to<unsigned>();
    }

    // (number->string number [radix] [style]) writes number in radix (10 by
    // default) in style: :decimal, :fraction, or :auto (the default, which
    // writes a decimal if it terminates and a fraction otherwise). The radix
    // and style can be given in either order.
    continuation_type number_to_string_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        constexpr std::string_view name{"number->string"};
        if (args.empty() or args.size() > 3) {
            raise_argument_error(name, args, std::format(
                "expected 1 to 3 arguments (number [radix] [style]), got {}", args.size()));
        }
        auto num_val = eval(args[0], env);
        auto num = std::get_if<bignum>(&num_val->data);
        if (not num) {
            raise_argument_error(name, args, std::format(
                "argument must be a number, got {}", value_to_string(num_val)));
        }

        std::optional<unsigned> radix;
        std::optional<number_style> style;
        for (const auto& arg: args | std::views::drop(1)) {
            auto val = eval(arg, env);
            if (std::holds_alternative<bignum>(val->data)) {
                if (radix) raise_argument_error(name, args, "more than one radix");
                radix = radix_argument(name, args, val);
            } else if (auto sym = std::get_if<symbol>(&val->data)) {
                if (style) raise_argument_error(name, args, "more than one style");
                if (":decimal" == sym->name) style = number_style::decimal;
                else if (":fraction" == sym->name) style = number_style::fraction;
                else if (":auto" == sym->name) style = number_style::automatic;
                else raise_argument_error(name, args, std::format(
                    "style must be :decimal, :fraction, or :auto, got {}", sym->name));
            } else {
                raise_argument_error(name, args, std::format(
                    "expected a radix or a style, got {}", value_to_string(val)));
            }
        }
        return value::make(format_number(*num,
            style.value_or(number_style::automatic), radix.value_or(10)));
    }

    // (string->number string [radix]) reads string as a number in radix (10 by
    // default), in any form number->string writes, and returns () if it isn't
    // one.
    continuation_type string_to_number_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        constexpr std::string_view name{"string->number"};
        if (args.empty() or args.size() > 2) {
            raise_argument_error(name, args, std::format(
                "expected 1 or 2 arguments (string [radix]), got {}", args.size()));
        }
        auto str_val = eval(args[0], env);
        auto str = std::get_if<std::string>(&str_val->data);
        if (not str) {
            raise_argument_error(name, args, std::format(
                "argument must be a string, got {}", value_to_string(str_val)));
        }
        unsigned radix = (2 == args.size())? radix_argument(name, args, eval(args[1], env)): 10;
        auto result = parse_number(*str, radix);
        return result? value::make(*result): value::make(nullptr);
    }

    continuation_type load_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args("load", args, 1, "(filename)");
        
        auto filename_val = eval(args[0], env);
        if (not std::holds_alternative<std::string>(filename_val->data)) {
            throw evaluation_error("load: filename must be a string", "load", call_stack::format());
        }
        
        auto filename = std::get<std::string>(filename_val->data);
        
        try {
            return load_file(filename, env);
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors unchanged
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("load: {}", e.what()),
                std::format("(load \"{}\")", filename),
                call_stack::format()
            );
        }
    }

    continuation_type read_operative(const std::vector<value_ptr>& args, env_ptr)
    {
        expect_args("read", args, 0);

        try {
            // The parser reads from stdin lazily, one expression at a time.
            static parser stdin_parser{std::cin};

            auto expr = stdin_parser.parse_expression();
            return expr;
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("read: {}", e.what()),
                "read",
                call_stack::format()
            );
        }
    }

    // Evaluate an operative's only argument, which must be an environment.
    env_ptr environment_argument(std::string_view name,
        const std::vector<value_ptr>& args, env_ptr env)
    {
        expect_args(name, args, 1);
        auto arg = unwrap_mutable_binding(eval(args[0], env));
        auto target = std::get_if<env_ptr>(&arg->data);
        if (not target) {
            throw evaluation_error(
                std::format("{}: argument must be an environment, got {}",
                    name, value_to_string(arg)),
                std::string{name},
                call_stack::format()
            );
        }
        return *target;
    }

    continuation_type environment_names_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        auto target = environment_argument("environment-names", args, env);
        auto result = value::make(nullptr);
        auto names = target->get_own_symbols();
        for (const auto& name: names | std::views::reverse) {
            result = value::make(cons_cell{value::make(symbol{name}), result});
        }
        return result;
    }

    continuation_type environment_parent_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        auto target = environment_argument("environment-parent", args, env);
        auto parent = target->get_parent();
        if (not parent) return value::make(nullptr);
        return value::make(parent);
    }

    // An operative that takes no arguments and returns target. It holds a
    // weak reference: the cycle collector can't see references inside a
    // builtin, so a strong one would keep target alive forever.
    auto make_environment_getter(std::string name, const env_ptr& target)
    {
        return [name, weak = std::weak_ptr<environment>{target}](
            const std::vector<value_ptr>& args, env_ptr) -> continuation_type
        {
            expect_args(name, args, 0);
            auto target = weak.lock();
            if (not target) {
                throw evaluation_error(
                    std::format("{}: the environment no longer exists", name),
                    name,
                    call_stack::format()
                );
            }
            return value::make(target);
        };
    }

} // namespace builtins

void add_church_boleans(env_ptr env)
{
    auto true_value = value::make(operative{
            param_pattern{false, {"x", "y"}},
            "env",
            builtins::make_eval_expression("x"),
            env,
            "true"});
    env->define("true", true_value);

    auto false_value = value::make(operative{
            param_pattern{false, {"x", "y"}},
            "env",
            builtins::make_eval_expression("y"),
            env,
            "false"});
    env->define("false", false_value);
}

// Create a top-level environment, for the library and the code that uses it.
// Its parent holds the builtins, so the library and user code can shadow a
// builtin but can't replace it.
env_ptr create_top_level_environment()
{
    auto builtins_env = environment::make();

    auto define_builtin = [builtins_env](const std::string& name, 
                    std::function<continuation_type(const std::vector<value_ptr>&, env_ptr)> func)
    {
        builtins_env->define(name, value::make(builtin_operative{name, std::move(func)}));
    };

    auto define_arithmetic = [define_builtin](const std::string& name,
                    std::function<bignum(bignum, bignum)> op, int identity,
                    bool identity_without_arguments)
    {
        define_builtin(name, builtins::make_arithmetic_operative(name, op,
            bignum{identity}, identity_without_arguments));
    };

    // Control
    define_builtin("vau", builtins::vau_operative);
    define_builtin("eval", builtins::eval_operative);
    define_builtin("eval-list", builtins::eval_list_operative);
    define_builtin("define", builtins::define_operative);
    define_builtin("invoke", builtins::invoke_operative);
    define_builtin("macro", builtins::macro_operative);
    define_builtin("try", builtins::try_operative);
    define_builtin("raise", builtins::raise_operative);
#define USE_PRIMITIVE_DO
#ifdef USE_PRIMITIVE_DO
    define_builtin("do", builtins::do_operative);
#endif
    define_builtin("load", builtins::load_operative);
    define_builtin("read", builtins::read_operative);
    // Arithmetic
    define_arithmetic("+", std::plus<bignum>{}, 0, true);
    define_arithmetic("-", std::minus<bignum>{}, 0, false);
    define_arithmetic("*", std::multiplies<bignum>{}, 1, true);
    define_arithmetic("/", std::divides<bignum>{}, 1, false);
    define_builtin("numerator", builtins::numerator_operative);
    define_builtin("denominator", builtins::denominator_operative);
    define_builtin("remainder", builtins::remainder_operative);
    // Numeric comparison
    define_builtin("<=>", builtins::spaceship_operative);
    // Lists
    define_builtin("cons", builtins::cons_operative);
    define_builtin("first", builtins::first_operative);
    define_builtin("rest", builtins::rest_operative);
    define_builtin("nil?", builtins::nil_p_operative);
    // Strings
    define_builtin("string->list", builtins::string_to_list_operative);
    define_builtin("list->string", builtins::list_to_string_operative);
    // Symbols
    define_builtin("string->symbol", builtins::string_to_symbol_operative);
    define_builtin("symbol->string", builtins::symbol_to_string_operative);
    // Numbers and strings
    define_builtin("number->string", builtins::number_to_string_operative);
    define_builtin("string->number", builtins::string_to_number_operative);
    // Equality
    define_builtin("=", builtins::equal_operative);
    // I/O
    define_builtin("write", builtins::write_operative);
    define_builtin("display", builtins::display_operative);
    define_builtin("flush", builtins::flush_operative);
    // Mutables
    define_builtin("define-mutable", builtins::define_mutable_operative);
    define_builtin("set!", builtins::set_operative);
    // Reflection
    define_builtin("typeof", builtins::typeof_operative);
    // Environments
    define_builtin("environment-names", builtins::environment_names_operative);
    define_builtin("environment-parent", builtins::environment_parent_operative);

    add_church_boleans(builtins_env);

    auto top_level = environment::make(builtins_env);
    define_builtin("get-builtins-environment",
        builtins::make_environment_getter("get-builtins-environment", builtins_env));
    define_builtin("get-top-level-environment",
        builtins::make_environment_getter("get-top-level-environment", top_level));
    return top_level;
}

void add_repl_bindings(env_ptr env)
{
    env->define("redefine",
        value::make(builtin_operative{"redefine", builtins::redefine_operative}));
}

// Bind parameters to operands in target environment
void bind_parameters(const param_pattern& params, value_ptr operands, env_ptr target_env)
{
    NOEVAL_DEBUG(env_binding, "Binding parameters: {} to operands: {}", 
            params.is_variadic? "variadic": "fixed", value_to_string(operands));

    if (params.is_variadic) {
        // Variadic case: bind all operands to the single parameter name
        if (1 != params.param_names.size()) {
            throw evaluation_error("Variadic parameter pattern must have exactly one parameter name");
        }
        NOEVAL_DEBUG(env_binding, "Binding variadic parameter '{}' to all operands", 
                  params.param_names[0]);
        target_env->define(params.param_names[0], operands);
    } else {
        // Fixed parameter case: convert operands to vector and bind individually
        auto operand_list = list_to_vector(operands);

        NOEVAL_DEBUG(env_binding, "Binding {} fixed parameters to {} operands", 
                  params.param_names.size(), operand_list.size());

        if (operand_list.size() != params.param_names.size()) {
            throw evaluation_error(
                std::format("Wrong number of arguments: expected {}, got {}",
                           params.param_names.size(), operand_list.size())
            );
        }
        
        // Bind each parameter to its corresponding operand
        for (const auto& [param_name, operand]: std::views::zip(params.param_names, operand_list)) {
            target_env->define(param_name, operand);
        }
    }
}

continuation_type operate_operative(const operative& op, value_ptr operands, env_ptr env)
{
    // Create new environment for the operative
    auto new_env = environment::make(op.closure_env);

    // Bind parameters to unevaluated operands
    try {
        bind_parameters(op.params, operands, new_env);
    } catch (const evaluation_error& e) {
        throw evaluation_error(std::format("{} {}", op.to_string(), e.message), e.context, call_stack::format());
    }

    // Bind environment parameter
    NOEVAL_DEBUG(env_binding, "Binding env parameter '{}' to environment {}", 
              op.env_param, to_string(env));
    // We use nil as the equivalent to Kernel's #ignore for vau's
    // environment parameter.
    if (not op.env_param.empty()) {
        new_env->define(op.env_param, value::make(env));
    }

    // Evaluate body in new environment
#if USE_TAIL_CALL
    return tail_call{op.body, new_env};
#else
    return eval(op.body, new_env);
#endif
}

continuation_type operate_builtin(const builtin_operative& op, value_ptr operands, env_ptr env)
{
    NOEVAL_DEBUG(builtin, "Invoking builtin '{}' with operands: {}", 
              op.name, value_to_string(operands));
    
    // Convert operands to vector for easier processing
    auto operand_list = list_to_vector(operands);
    
    NOEVAL_DEBUG(builtin, "Converted to {} arguments", operand_list.size());
    
    // Call the built-in function with the evaluated operands
    auto result = op.func(operand_list, env);
    
    NOEVAL_DEBUG(builtin, "Builtin '{}' returned: {}", op.name, value_to_string(result));
    return result;
}

value_ptr eval_symbol(const symbol& sym, env_ptr env)
{
    // Look up the symbol in the environment
    try {
        auto binding = env->lookup(sym.name);
        
        // If it's a mutable binding, return the wrapped value
        if (std::holds_alternative<mutable_binding>(binding->data)) {
            return std::get<mutable_binding>(binding->data).value;
        }
        
        return binding;
    } catch (const std::exception& e) {
        throw evaluation_error(e.what(), sym.name, call_stack::format());
    }
}

// Call a macro's transformer with a combination's unevaluated operands and
// return the expansion. The transformer gets a new, empty environment, so the
// expansion can depend only on the operands.
value_ptr expand_macro(const macro& m, value_ptr operands)
{
    auto call = value::make(cons_cell{m.transformer, std::move(operands)});
    return eval(call, environment::make());
}

// expr is the combination being evaluated, and cell is its cons_cell.
continuation_type eval_operation(const value_ptr& expr, const cons_cell& cell, env_ptr env)
{
    auto operator_expr = cell.car;
    auto operands = cell.cdr;
    
    // Check if operator is already an operative or macro value. Evaluating it
    // would return it anyway, so this is only a shortcut, but it's taken by
    // every embedded operator, such as those in macro expansions and wrap's
    // calls.
    value_ptr op;
    if (std::holds_alternative<operative>(operator_expr->data) or
        std::holds_alternative<builtin_operative>(operator_expr->data) or
        std::holds_alternative<macro>(operator_expr->data)) {
        // Use the operative or macro directly
        op = operator_expr;
    } else {
        // Evaluate the operator expression
        op = eval(operator_expr, env);
    }

    // A combination whose operator is no longer a macro doesn't need its
    // cached expansion, which would keep the transformer (and its closure
    // environment) alive. The cache is almost always empty, so this is
    // usually just a null check.
    if (cell.expansion_cache.cache and not std::holds_alternative<macro>(op->data)) {
        NOEVAL_DEBUG(macro, "Clearing cached expansion of {}", expr_context(expr));
        cell.expansion_cache.cache.reset();
    }

    // Check if it's an operative
    if (std::holds_alternative<operative>(op->data)) {
        return operate_operative(std::get<operative>(op->data), operands, env);
    }

    // Check if it's a builtin operative
    if (std::holds_alternative<builtin_operative>(op->data)) {
        return operate_builtin(std::get<builtin_operative>(op->data), operands, env);
    }

    // Check if it's a macro. Use the expansion cached on this combination if
    // it came from the same transformer.
    if (auto m = std::get_if<macro>(&op->data)) {
        value_ptr expansion;
        auto& cache = cell.expansion_cache.cache;
        if (cache and cache->transformer == m->transformer) {
            NOEVAL_DEBUG(macro, "Using cached expansion of {}", expr_context(expr));
            expansion = cache->expansion;
        } else {
            expansion = expand_macro(*m, operands);
            NOEVAL_DEBUG(macro, "Expanded {} to {}", expr_context(expr), value_to_string(expansion));
            cache = std::make_unique<macro_cache>(m->transformer, expansion);
        }
#if USE_TAIL_CALL
        return tail_call{expansion, env};
#else
        return eval(expansion, env);
#endif
    }

    throw evaluation_error(
        std::format("Not an operative: {}", value_to_string(op)),
        expr_context(expr),
        call_stack::format()
    );
}

///////////////////////////////////////////////////////////////////////////////

// Trial deletion cycle collector (see design/gc-plan.md)
//
// Every cycle in Noeval passes through an environment, so we scan everything
// reachable from the registered environments. Each environment and value we
// find starts with its reference count, and we subtract one for every reference
// to it that we find inside the scanned heap. Anything left with a positive
// count is also referenced from outside the heap (e.g. from the C++ stack), so
// it is a root. Environments that aren't reachable from a root are garbage.
//
// If the scan misses a reference, the thing it refers to looks like it is
// referenced from outside the heap, so it is kept. A mistake here causes a
// leak, not a corrupted environment. Counting a reference twice, however, could
// cause live environments to be collected, so each reference must be counted
// exactly once.
struct cycle_collector {
    std::unordered_map<environment*, long> env_refs;
    std::unordered_map<value*, long> value_refs;

    // Call on_env or on_value for each environment or value that env holds a
    // reference to.
    static void for_each_reference(environment* env, auto on_env, auto on_value)
    {
        if (env->parent) on_env(env->parent.get());
        for (const auto& binding: env->bindings) {
            if (binding.second) on_value(binding.second.get());
        }
    }

    // Call on_env or on_value for each environment or value that v holds a
    // reference to.
    //
    // Builtin operatives could hold references in their std::function, but we
    // can't see those. That's OK. (It will cause a leak rather than a
    // collection of something in use.)
    static void for_each_reference(value* v, auto on_env, auto on_value)
    {
        if (auto env = std::get_if<env_ptr>(&v->data)) {
            if (*env) on_env(env->get());
        } else if (auto cell = std::get_if<cons_cell>(&v->data)) {
            if (cell->car) on_value(cell->car.get());
            if (cell->cdr) on_value(cell->cdr.get());
            if (auto& cache = cell->expansion_cache.cache) {
                if (cache->transformer) on_value(cache->transformer.get());
                if (cache->expansion) on_value(cache->expansion.get());
            }
        } else if (auto op = std::get_if<operative>(&v->data)) {
            if (op->closure_env) on_env(op->closure_env.get());
            if (op->body) on_value(op->body.get());
        } else if (auto m = std::get_if<macro>(&v->data)) {
            if (m->transformer) on_value(m->transformer.get());
        } else if (auto mb = std::get_if<mutable_binding>(&v->data)) {
            if (mb->value) on_value(mb->value.get());
        }
    }

    // Find the reference count of each environment and value that isn't
    // accounted for by references from inside the heap.
    void count_external_references()
    {
        for (auto env: environment::registry) {
            // An environment that isn't owned by a shared_ptr yet (or anymore)
            // can't be referenced by anything, so we leave it out.
            auto refs = env->weak_from_this().use_count();
            if (refs > 0) env_refs[env] = refs;
        }

        std::vector<value*> pending;
        auto on_env = [&](environment* env) {
            auto iter = env_refs.find(env);
            if (iter != env_refs.end()) --(iter->second);
        };
        auto on_value = [&](value* v) {
            auto [iter, inserted] = value_refs.try_emplace(v, 0);
            if (inserted) {
                iter->second = v->weak_from_this().use_count();
                pending.push_back(v);
            }
            --(iter->second);
        };

        for (const auto& entry: env_refs) {
            for_each_reference(entry.first, on_env, on_value);
        }
        while (not pending.empty()) {
            auto v = pending.back();
            pending.pop_back();
            for_each_reference(v, on_env, on_value);
        }

        auto negative = [](const auto& entry) { return entry.second < 0; };
        if (std::ranges::any_of(env_refs, negative) or std::ranges::any_of(value_refs, negative)) {
            throw std::logic_error("Garbage collector found more references than the reference count");
        }
    }

    // Find the environments reachable from roots (the environments and values
    // that are referenced from outside the heap).
    std::unordered_set<environment*> find_live_environments() const
    {
        std::unordered_set<environment*> live_envs;
        std::unordered_set<value*> live_values;
        std::vector<environment*> env_work;
        std::vector<value*> value_work;

        auto on_env = [&](environment* env) {
            if (not env_refs.contains(env)) return;
            if (live_envs.insert(env).second) env_work.push_back(env);
        };
        auto on_value = [&](value* v) {
            if (live_values.insert(v).second) value_work.push_back(v);
        };

        for (const auto& [env, refs]: env_refs) {
            if (refs > 0) on_env(env);
        }
        for (const auto& [v, refs]: value_refs) {
            if (refs > 0) on_value(v);
        }

        while (not (env_work.empty() and value_work.empty())) {
            if (not env_work.empty()) {
                auto env = env_work.back();
                env_work.pop_back();
                for_each_reference(env, on_env, on_value);
            } else {
                auto v = value_work.back();
                value_work.pop_back();
                for_each_reference(v, on_env, on_value);
            }
        }
        return live_envs;
    }

    // Returns the number of environments collected
    size_t collect()
    {
        count_external_references();
        auto live_envs = find_live_environments();

        // Hold on to the garbage while we break the cycles so that nothing gets
        // destroyed until we're done.
        std::vector<env_ptr> garbage;
        for (const auto& entry: env_refs) {
            if (not live_envs.contains(entry.first)) {
                garbage.push_back(entry.first->shared_from_this());
            }
        }
        env_refs.clear();
        value_refs.clear();

        for (auto& env: garbage) {
            NOEVAL_DEBUG(gc, "Collecting environment: {}", to_string(env));
            env->bindings.clear();
            env->parent.reset();
        }
        return garbage.size();
    }
};

// Collect if enough environments have been created since the last
// collection. Collection can happen here, in the middle of an evaluation, so
// C++ code must hold environments and values it is using by shared_ptr.
void environment::maybe_collect()
{
    auto interval = (stress_interval > 0)?
        stress_interval: std::max(min_collection_interval, survivors);
    if (created_since_collection >= interval) {
        collect();
    }
    ++created_since_collection;
}

env_ptr environment::make()
{
    maybe_collect();
    return env_ptr(new environment);
}

env_ptr environment::make(env_ptr parent)
{
    maybe_collect();
    return env_ptr(new environment(std::move(parent)));
}

void environment::collect()
{
    NOEVAL_DEBUG(gc, "Before collection: Undestructed environments: {}", environment::get_constructed_count());
    NOEVAL_DEBUG(gc, "Before collection: Registered environments  : {}", environment::get_registered_count());
    auto collected = cycle_collector{}.collect();
    NOEVAL_DEBUG(gc, "Collected environments   : {}", collected);
    created_since_collection = 0;
    survivors = count;
    NOEVAL_DEBUG(gc, "After collection : Undestructed environments: {}", environment::get_constructed_count());
    NOEVAL_DEBUG(gc, "After collection : Registered environments  : {}", environment::get_registered_count());
}

///////////////////////////////////////////////////////////////////////////////

struct timer
{
    // Could use high_resolution_clock if necessary
    using clock = std::chrono::steady_clock;

    timer(std::string_view name): name{name}, before{clock::now()} {}

    ~timer()
    {
        using units = std::chrono::milliseconds;
        auto after{clock::now()};
        auto duration {
            std::chrono::duration_cast<units>(after - before).count()
        };
        NOEVAL_DEBUG(timer, "{}: {} ms", name, duration);
    }

private:
    std::string name;
    clock::time_point before;
};

value_ptr eval(value_ptr expr, env_ptr env)
{
    call_stack::guard g(expr);
    while (true) {
        NOEVAL_DEBUG(eval, "{}[{}] Evaluating({}): {}", 
            call_stack::indent(), 
            call_stack::depth(),
            value_type_string(expr),
            value_to_string(expr));
        try {
            continuation_type k = std::visit([&](const auto& v) -> continuation_type {
                using T = std::decay_t<decltype(v)>;
                
                // As in Kernel, symbols and cons cells are evaluated, and
                // everything else evaluates to itself, so code built at
                // runtime can contain any value. The exception is
                // mutable_binding, which symbol lookup always unwraps, so
                // reaching it here would be an interpreter bug.
                if constexpr (std::is_same_v<T, symbol>) {
                    return eval_symbol(v, env);
                } else if constexpr (std::is_same_v<T, cons_cell>) {
                    return eval_operation(expr, v, env);
                } else if constexpr (std::is_same_v<T, mutable_binding>) {
                    throw evaluation_error(
                        std::format("Cannot evaluate {}", demangle<T>()),
                        expr_context(expr),
                        call_stack::format()
                    );
                } else {
                    return expr;
                }
            }, expr->data);
            if (auto tc{std::get_if<tail_call>(&k)}) {
                expr = tc->expr;
                env = tc->env;
                g.tail_call(expr);
                NOEVAL_DEBUG(tco, "Tail call!");
                continue;
            }
            auto result{std::get<value_ptr>(k)};
            NOEVAL_DEBUG(eval, "{}[{}] Result: {}", 
                    call_stack::indent(),
                    call_stack::depth(), 
                    value_to_string(result));
            return result;
        } catch (const evaluation_error& e) {
            // If it is already an evaluation error,
            // rethrow before the following catch grabs it.
            throw;
        } catch (const std::exception& e) {
            throw evaluation_error(e.what(), expr_context(expr), call_stack::format());
        }
    }
    return nullptr; // Unreachable
}

value_ptr top_level_eval(value_ptr expr, env_ptr env)
{
    if (NOEVAL_DEBUG_ENABLED(timer)) {
        timer eval_timer{"eval"};
        return eval(expr, env);
    }
    return eval(expr, env);
}

// The files being loaded, innermost last
static std::vector<std::filesystem::path> loading_files;

value_ptr load_file(const std::string& filename, env_ptr env)
{
    std::filesystem::path path{filename};
    if (path.is_relative() and not loading_files.empty()) {
        path = loading_files.back().parent_path() / path;
    }

    auto content = read_file_content(path.string());
    parser p(content, path.string());
    auto expressions = p.parse_all();

    loading_files.push_back(path);
    struct pop_guard {
        ~pop_guard() { loading_files.pop_back(); }
    } pop;

    auto result = value::make(nullptr);
    for (const auto& expr: expressions) {
        result = top_level_eval(expr, env);
    }
    return result;
}

bool execute_script(const std::string& filename, env_ptr env)
{
    try {
        //TODO: Should our result code be determined by the result of the
        //      last expression?
        load_file(filename, env);
        return true;
    } catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return false;
    }
}

bool load_library_file(const std::string& filename, env_ptr env)
{
    std::println("Loading library: {}", filename);
    try {
        load_file(filename, env);
    } catch (const std::exception& e) {
        std::println("Error loading library: {}", e.what());
        return false;
    }
    std::println("Library loaded successfully.\n");
    return true;
}

// Function to run library tests from file
int run_library_tests(env_ptr outer_env)
{
    std::println("Running library tests from file...");

    // Make an isolated test environment
    auto env = environment::make(outer_env);

    value_ptr result;
    try {
        result = load_file("tests/main.noeval", env);
    } catch (const std::exception& e) {
        std::println("\nError: {}", e.what());
        println_red("\n✗ Exception caught");
        return 1;
    }

    // The last expression should be the test result
    auto result_str = value_to_string(result);
    if ("\"All library tests passed!\"" == result_str) {
        std::println("\n✓ {}", result_str.substr(1, result_str.length() - 2)); // Remove quotes
        return 0;
    }
    println_red("\n✗ Library tests failed with result: {}", result_str);
    return 1;
}

// Ask whether to continue despite test failures. Only ask when stdin is a
// terminal. Otherwise the prompt would consume a line of piped input, so
// don't continue.
bool confirm_continue(std::string_view message)
{
    if (not isatty(STDIN_FILENO)) return false;

    std::print("{} Do you want to continue anyway? (y/N): ", message);
    std::string response;
    std::getline(std::cin, response);

    // Convert to lowercase for comparison
    std::ranges::transform(response, response.begin(), ::tolower);

    return ("y" == response) or ("yes" == response);
}

env_ptr reload_top_level_environment(bool test_the_library)
{
    // Create environment and load library
    auto env = create_top_level_environment();
    environment::collect();

    // Load standard library
    std::println("Loading standard library...");
    bool library_ok = load_library_file("src/lib.noeval", env);
    if (not library_ok) {
        std::println("Loading the library failed!");
        return env_ptr{nullptr};
    }

    if (test_the_library) {
        int failures{0};
        // Run library tests after loading
        std::println("\n{}", std::string(60, '='));
        std::println("Running library tests...");
        failures += run_library_tests(env);
        std::println("{}", std::string(60, '='));
        if (0 != failures) {
            println_red("\n✗ library tests failed!");
            
            if (not confirm_continue("Library tests failed.")) {
                std::println("Aborting due to library test failures.");
                return env_ptr{nullptr};
            }
            
            std::println("Continuing despite library test failures...");
        } else {
            std::println("\n✓ All tests passed!");
        }
    }

    return env;
}

// Print the process's peak memory use (resident set size) to stderr, as
// "peak memory: N kB". It reads VmHWM from /proc/self/status, which covers
// only this program. (getrusage's maximum also counts the memory of the
// process that started this one.)
void report_peak_memory()
{
    std::ifstream status("/proc/self/status");
    for (std::string line; std::getline(status, line); ) {
        if (line.starts_with("VmHWM:")) {
            auto value = line.substr(line.find_first_not_of(" \t", 6));
            // So that the report comes after the program's own output
            std::fflush(stdout);
            std::println(stderr, "peak memory: {}", value);
            return;
        }
    }
}

int main(const int argc, const char** argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);

    // Stress mode: NOEVAL_GC_STRESS=n collects every n environment creations.
    // (Unset, empty, or 0 means stress mode is off.)
    if (auto stress = std::getenv("NOEVAL_GC_STRESS")) {
        environment::set_stress_interval(std::max(0, std::atoi(stress)));
    }

    // NOEVAL_REPORT_PEAK_MEMORY (set to anything) reports the peak memory use
    // at exit, for the benchmarks.
    if (std::getenv("NOEVAL_REPORT_PEAK_MEMORY")) std::atexit(report_peak_memory);

    // Run only the garbage collection tests.
    if ((not args.empty()) and ("--gc-tests" == args[0])) {
        auto failures = run_gc_tests();
        environment::collect();
        return (0 == failures)? EXIT_SUCCESS: EXIT_FAILURE;
    }

    // Run the C++ tests and the library tests, then exit.
    if ((not args.empty()) and ("--tests" == args[0])) {
        bool ok = run_tests();
        {
            auto env = reload_top_level_environment(false);
            ok = env and (0 == run_library_tests(env)) and ok;
        }
        environment::collect();
        if (ok) {
            std::println("\n✓ All tests passed!");
        } else {
            println_red("\n✗ Tests failed!");
        }
        return ok? EXIT_SUCCESS: EXIT_FAILURE;
    }

    // Skip the C++ tests, e.g. so they don't count toward a benchmark's time.
    bool skip_tests{false};
    if ((not args.empty()) and ("--skip-tests" == args[0])) {
        skip_tests = true;
        args.erase(args.begin());
    }

    if ((not skip_tests) and (not run_tests())) {
        if (not confirm_continue("Tests failed.")) {
            std::println("Exiting due to test failures.");
            return EXIT_FAILURE;
        }
        
        std::println("Continuing despite test failures...");
    }

    int exit_status{EXIT_SUCCESS};

    // A scope for the environment to ensure it is destructed before our final
    // environment::collect() call.
    {
        // Create top-level environment and load library
        // The library tests take long enough that we're not going to do them on
        // startup. A :reload can be used to run them.
        auto env = reload_top_level_environment(false);
        if (not env) return EXIT_FAILURE;

        if (args.empty()) {
            std::println("Starting REPL...");
            repl(env);
        } else {
            if (not execute_script(args[0], env)) exit_status = EXIT_FAILURE;
        }
    }

    environment::collect();

#ifdef TEST_FOR_MOVE_ONLY_FUNCTION
    std::println("---");
    std::println("move_only_function support: {}", 
                  __cpp_lib_move_only_function >= 202110L? "available": "not available");
    std::println("copyable_function support: {}", 
                  __cpp_lib_copyable_function >= 202306L? "available": "not available");
    std::println("function_ref support: {}", 
                  __cpp_lib_function_ref >= 202306L? "available": "not available");
#endif
    return exit_status;
}

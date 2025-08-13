// Interpreter for a language built from fexprs and vau
// Code style will use snake_case and other conventions of the standard library

// NOTE THAT nil IS SPELT ()

#include <algorithm>
#include <format>
#include <functional>
#include <memory>
#include <print>
#include <ranges>
#include <string_view>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "debug.hpp"
#include "noeval.hpp"
#include "parser.hpp"
#include "repl.hpp"
#include "tests.hpp"
#include "utils.hpp"

std::string to_string(int value) { return std::to_string(value); }

std::string to_string(const std::string& value)
{
    std::string result = "\"";
    for (char c : value) {
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

std::string value_type_string(const value_ptr& val)
{
    return std::visit([](const auto& v) -> std::string {
        return demangle<decltype(v)>();
    }, val->data);
}

std::string cons_cell::to_string() const
{
    // Reconstruct the value_ptr for this cons_cell to reuse value_to_string
    auto this_val = std::make_shared<value>(*this);
    
    std::string result = "(";
    auto current = this_val;
    bool first = true;
    while (std::holds_alternative<cons_cell>(current->data)) {
        if (!first) result += " ";
        first = false;
        auto& cell = std::get<cons_cell>(current->data);
        result += value_to_string(cell.car);
        current = cell.cdr;
    }
    if (!std::holds_alternative<std::nullptr_t>(current->data)) {
        result += " . " + value_to_string(current);
    }
    result += ")";
    return result;
}

std::string operative::to_string() const
{
    if (not tag.empty()) return tag;
    // It isn't easy (yet) to change the delimiter that format uses for ranges,
    // so explicitly use std::views::join_with.
    return std::format("(operative {}{:s}{} {} {})",
        params.is_variadic? "": "(",
        params.param_names | std::views::join_with(' '),
        params.is_variadic? "": ")",
        env_param,
        value_to_string(body)
    );
}

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

// Environment implementation
value_ptr environment::lookup(const std::string& name) const
{
    NOEVAL_DEBUG(env_lookup, "Looking up '{}' in env {}", name, static_cast<const void*>(this));
    
    if (NOEVAL_DEBUG_ENABLED(env_dump)) {
        NOEVAL_DEBUG(env_dump, "Current bindings:");
        for (const auto& [key, value] : bindings) {
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

std::string environment::dump_chain() const
{
    std::string chain = std::format("{}", static_cast<const void*>(this));
    if (parent) {
        chain += " -> " + parent->dump_chain();
    }
    return chain;
}

// Forward declaration for eval
value_ptr eval(value_ptr expr, env_ptr env);

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
    if (!is_cons(val)) {
        throw std::runtime_error("car: not a cons cell");
    }
    return std::get<cons_cell>(val->data).car;
}

// Get cdr of a cons cell
value_ptr cdr(const value_ptr& val)
{
    if (!is_cons(val)) {
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
    if (!is_nil(current)) {
        throw std::runtime_error("Improper list");
    }
    return result;
}

// Helper to construct a Noeval list from C++ values
value_ptr make_list(std::initializer_list<value_ptr> elements)
{
    value_ptr result = std::make_shared<value>(nullptr); // Start with nil
    
    // Build list backwards
    for (auto it = elements.end(); it != elements.begin(); ) {
        --it;
        result = std::make_shared<value>(cons_cell{*it, result});
    }
    
    return result;
}

value_ptr quote(value_ptr expr)
{
    return make_list({
        std::make_shared<value>(symbol{"q"}),
        expr
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
        if (!std::holds_alternative<symbol>(param->data)) {
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
value_ptr operate_operative(const operative& op, value_ptr operands, env_ptr env);
value_ptr operate_builtin(const builtin_operative& op, value_ptr operands, env_ptr env);

struct call_stack {
private:
    inline static thread_local std::vector<std::string> stack;
public:
    struct guard {
        guard(value_ptr expr)
        {
            stack.push_back(value_to_string(expr));
        }
        ~guard()
        {
            if (!stack.empty()) {
                stack.pop_back();
            }
        }
    };

    static std::string format()
    {
        std::string result;
        for (const auto& [index, line] : stack | std::views::enumerate) {
            result += std::format("{}: {}\n", index, line);
        }
        return result;
    }

    static size_t depth() { return stack.size(); }
    static std::string indent() { return std::string(depth() * 2, ' '); }
};

// Built-in operatives
namespace builtins {

    value_ptr vau_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 3) {
            throw evaluation_error(
                std::format("vau: expected 3 arguments (params env-param body), got {}", args.size()),
                std::format("(vau {} {} {})", 
                    args.size() > 0 ? expr_context(args[0]) : "?",
                    args.size() > 1 ? expr_context(args[1]) : "?",
                    args.size() > 2 ? expr_context(args[2]) : "?"),
                call_stack::format()
            );
        }
        
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
                if (!std::holds_alternative<symbol>(env_param_expr->data)) {
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
            return std::make_shared<value>(operative{
                std::move(param_pattern),
                std::move(env_param_name),
                body_expr,
                env
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
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("eval: expected 2 arguments (expr env), got {}", args.size()),
                args.empty() ? "(eval)" : 
                args.size() == 1 ? std::format("(eval {})", expr_context(args[0])) :
                std::format("(eval {} {} ...)", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
    }

    // Helper function to evaluate both arguments in current environment
    std::pair<value_ptr, value_ptr> evaluate_eval_arguments(const std::vector<value_ptr>& args, env_ptr env)
    {
        auto expr = args[0];          // Expression to evaluate (unevaluated)
        auto env_expr = args[1];      // Environment expression (unevaluated)

        NOEVAL_DEBUG(operative, "eval_operative called in environment {}", 
                  static_cast<const void*>(env.get()));
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
        if (!std::holds_alternative<env_ptr>(env_val->data)) {
            throw evaluation_error(
                std::format("eval: second argument must evaluate to an environment, got {}",
                        value_to_string(env_val)),
                std::format("(eval {} {})", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        
        auto target_env = std::get<env_ptr>(env_val->data);
        NOEVAL_DEBUG(operative, "Target environment for evaluation: {}", 
                  static_cast<const void*>(target_env.get()));
        
        return target_env;
    }

    // Helper function to perform final evaluation in target environment
    value_ptr evaluate_in_target_environment(value_ptr evaluated_expr, env_ptr target_env)
    {
        NOEVAL_DEBUG(operative, "About to evaluate {} in target environment", 
                  value_to_string(evaluated_expr));
        
        return eval(evaluated_expr, target_env);
    }

    // Evaluates both arguments, then evaluates the result of evaluating the
    // first argument in the environment evaluated from the second argument.
    value_ptr eval_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        try {
            validate_eval_arguments(args);
            
            auto [evaluated_expr, env_val] = evaluate_eval_arguments(args, env);
            
            auto target_env = extract_target_environment(env_val, args);
            
            return evaluate_in_target_environment(evaluated_expr, target_env);
            
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
    value_ptr define_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("define: expected 2 arguments (symbol value), got {}", args.size()),
                args.empty() ? "(define)" :
                args.size() == 1 ? std::format("(define {})", expr_context(args[0])) :
                std::format("(define {} {} ...)", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (!std::holds_alternative<symbol>(sym_expr->data)) {
            throw evaluation_error(
                std::format("define: first argument must be a symbol, got {}", expr_context(sym_expr)),
                std::format("(define {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
        
        try {
            auto sym_name = std::get<symbol>(sym_expr->data).name;
            auto val = eval(val_expr, env);
            
            env->define(sym_name, val);
            return val;
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            throw evaluation_error(
                std::format("define: {}", e.what()),
                std::format("(define {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
    }

    // Helper function to validate and extract integer from value
    int extract_integer(const value_ptr& val, const std::string& op_name, const value_ptr& original_arg)
    {
        if (!std::holds_alternative<int>(val->data)) {
            throw evaluation_error(
                std::format("{}: argument must be an integer, got {}", 
                           op_name, value_to_string(val)),
                std::format("... {} ...", expr_context(original_arg)),
                call_stack::format()
            );
        }
        return std::get<int>(val->data);
    }

    // Helper function to evaluate and validate the first argument
    int evaluate_first_argument(const value_ptr& first_arg, const std::string& op_name, env_ptr env)
    {
        auto first_val = eval(first_arg, env);
        if (!std::holds_alternative<int>(first_val->data)) {
            throw evaluation_error(
                std::format("{}: argument must be an integer, got {}", 
                           op_name, value_to_string(first_val)),
                std::format("({} {} ...)", op_name, expr_context(first_arg)),
                call_stack::format()
            );
        }
        return std::get<int>(first_val->data);
    }

    // Helper function to build error context for arithmetic operations
    std::string build_arithmetic_context(const std::string& op_name, const std::vector<value_ptr>& args)
    {
        std::string context = "(" + op_name;
        for (const auto& arg : args) {
            context += " " + expr_context(arg);
        }
        context += ")";
        return context;
    }

    auto make_arithmetic_operative(const std::string& op_name, std::function<int(int, int)> op)
    {
        return [op_name, op](const std::vector<value_ptr>& args, env_ptr env) {
            if (args.empty()) {
                throw evaluation_error(
                    std::format("{}: requires at least one argument", op_name),
                    std::format("({})", op_name),
                    call_stack::format()
                );
            }
            
            try {
                int initial_value = evaluate_first_argument(args[0], op_name, env);

                int result = std::ranges::fold_left(args | std::views::drop(1),
                    initial_value,
                    [op, op_name, &env](int accumulator, const value_ptr& arg)
                    {
                        auto val = eval(arg, env);
                        int operand = extract_integer(val, op_name, arg);
                        return op(accumulator, operand);
                    });
                    
                return std::make_shared<value>(result);
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
    value_ptr cons_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("cons: expected 2 arguments (first rest), got {}", args.size()),
                args.empty() ? "(cons)" : 
                args.size() == 1 ? std::format("(cons {})", expr_context(args[0])) :
                std::format("(cons {} {} ...)", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }
        
        auto first_val = eval(args[0], env);
        auto rest_val = eval(args[1], env);
        
        return std::make_shared<value>(cons_cell{first_val, rest_val});
    }

    // Evaluates argument
    value_ptr first_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 1) {
            throw evaluation_error(
                std::format("first: expected 1 argument, got {}", args.size()),
                args.empty() ? "(first)" : std::format("(first {} ...)", expr_context(args[0])),
                call_stack::format()
            );
        }
        
        auto val = eval(args[0], env);
        return car(val);  // Uses existing helper
    }

    // Evaluates argument
    value_ptr rest_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 1) {
            throw evaluation_error(
                std::format("rest: expected 1 argument, got {}", args.size()),
                args.empty() ? "(rest)" : std::format("(rest {} ...)", expr_context(args[0])),
                call_stack::format()
            );
        }
        
        auto val = eval(args[0], env);
        return cdr(val);  // Uses existing helper
    }

    // Helper to build `(eval symbol env)`
    value_ptr make_eval_expression(std::string_view symbol_name)
    {
        return std::make_shared<value>(cons_cell{
            std::make_shared<value>(symbol{"eval"}),
            std::make_shared<value>(cons_cell{
                std::make_shared<value>(symbol{symbol_name}),
                std::make_shared<value>(cons_cell{
                    std::make_shared<value>(symbol{"env"}),
                    std::make_shared<value>(nullptr)  // nil
                })
            })
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
    value_ptr nil_p_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 1) {
            throw evaluation_error(
                std::format("nil?: expected 1 argument, got {}", args.size()),
                args.empty() ? "(nil?)" : std::format("(nil? {} ...)", expr_context(args[0])),
                call_stack::format()
            );
        }
        
        auto val = eval(args[0], env);
        return is_nil(val)? church_true(env): church_false(env);
    }

    value_ptr invoke_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("invoke: expected 2 arguments (operative arg-list), got {}", args.size()),
                "invoke",
                call_stack::format()
            );
        }

        auto op_expr = args[0];  // The operative to invoke (unevaluated)
        auto arg_list = eval(args[1], env);  // The list of arguments
        
        // Convert the argument list to a vector
        auto arg_vector = list_to_vector(arg_list);
        
        // Create a new expression: (operative arg1 arg2 ...)
        auto call_expr = std::make_shared<value>(cons_cell{op_expr, arg_list});
        
        return eval(call_expr, env);
    }

    // Evaluates each argument
    value_ptr do_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.empty()) {
            // Empty do returns nil
            return std::make_shared<value>(nullptr);
        }
        
        try {
            value_ptr result = std::make_shared<value>(nullptr); // default to nil
            
            // Evaluate each expression in sequence, keeping the last result
            for (const auto& expr : args) {
                result = eval(expr, env);
            }
            
            return result;
        } catch (const evaluation_error&) {
            throw; // Re-throw evaluation errors as-is
        } catch (const std::exception& e) {
            // Build context showing all expressions
            std::string context = "(do";
            for (const auto& arg : args) {
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

    // Evaluates its arguments
    // Most similar to Kernel's `equal?`
    // Doesn't work for all types yet.
    value_ptr equal_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("=: expected 2 arguments, got {}", args.size()),
                args.empty() ? "(=)" : 
                args.size() == 1 ? std::format("(= {})", expr_context(args[0])) :
                std::format("(= {} {} ...)", expr_context(args[0]), expr_context(args[1])),
                call_stack::format()
            );
        }

        auto val1 = eval(args[0], env);
        auto val2 = eval(args[1], env);

        // Simple equality check for integers and nil
        if (std::holds_alternative<int>(val1->data) && std::holds_alternative<int>(val2->data)) {
            bool equal = std::get<int>(val1->data) == std::get<int>(val2->data);
            return equal? church_true(env): church_false(env);
        }
        
        if (std::holds_alternative<std::nullptr_t>(val1->data) && std::holds_alternative<std::nullptr_t>(val2->data)) {
            return church_true(env); // Both nil
        }

        if (std::holds_alternative<std::string>(val1->data) && std::holds_alternative<std::string>(val2->data)) {
            bool equal = std::get<std::string>(val1->data) == std::get<std::string>(val2->data);
            return equal? church_true(env): church_false(env);
        }

        if (std::holds_alternative<symbol>(val1->data) && std::holds_alternative<symbol>(val2->data)) {
            bool equal = std::get<symbol>(val1->data).name == std::get<symbol>(val2->data).name;
            return equal? church_true(env): church_false(env);
        }

        // Different types or unsupported comparison
        return church_false(env); // false
    }

    value_ptr write_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 1) {
            throw evaluation_error(
                std::format("write: expected 1 argument, got {}", args.size()),
                args.empty() ? "(write)" : std::format("(write {} ...)", expr_context(args[0])),
                call_stack::format()
            );
        }
        
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

    value_ptr display_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 1) {
            throw evaluation_error(
                std::format("display: expected 1 argument, got {}", args.size()),
                args.empty() ? "(display)" : std::format("(display {} ...)", expr_context(args[0])),
                call_stack::format()
            );
        }
        
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

    value_ptr define_mutable_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("define-mutable: expected 2 arguments (symbol value), got {}", args.size()),
                "define-mutable",
                call_stack::format()
            );
        }
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (!std::holds_alternative<symbol>(sym_expr->data)) {
            throw evaluation_error(
                "define-mutable: first argument must be a symbol",
                std::format("(define-mutable {} {})", expr_context(sym_expr), expr_context(val_expr)),
                call_stack::format()
            );
        }
        
        try {
            auto sym_name = std::get<symbol>(sym_expr->data).name;
            auto val = eval(val_expr, env);
            
            // Wrap the value in a mutable_binding
            auto mutable_val = std::make_shared<value>(mutable_binding{val});
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

    value_ptr set_operative(const std::vector<value_ptr>& args, env_ptr env)
    {
        if (args.size() != 2) {
            throw evaluation_error(
                std::format("set!: expected 2 arguments (symbol value), got {}", args.size()),
                "set!",
                call_stack::format()
            );
        }
        
        auto sym_expr = args[0];
        auto val_expr = args[1];
        
        if (!std::holds_alternative<symbol>(sym_expr->data)) {
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
            if (!std::holds_alternative<mutable_binding>(current_binding->data)) {
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
    value_ptr try_operative(const std::vector<value_ptr>& args, env_ptr env)
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
                std::make_shared<value>(symbol{"error"}),
                std::make_shared<value>(e.message),
                std::make_shared<value>(e.context),
                std::make_shared<value>(e.stack_trace)
            });
        } catch (const std::exception& e) {
            error_val = make_list({
                std::make_shared<value>(symbol{"error"}),
                std::make_shared<value>(e.what()),
                std::make_shared<value>(std::string{}),  // context
                std::make_shared<value>(std::string{})   // stack trace
            });
        } catch (...) {
            error_val = make_list({
                std::make_shared<value>(symbol{"error"}),
                std::make_shared<value>(std::string{"unknown error"}),
                std::make_shared<value>(std::string{}),  // context
                std::make_shared<value>(std::string{})   // stack trace
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
                quote(error_val)
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

} // namespace builtins

void add_church_boleans(env_ptr env)
{
    auto true_value = std::make_shared<value>(operative{
            param_pattern{false, {"x", "y"}},
            "env",
            builtins::make_eval_expression("x"),
            env,
            "true"});
    env->define("true", true_value);

    auto false_value = std::make_shared<value>(operative{
            param_pattern{false, {"x", "y"}},
            "env",
            builtins::make_eval_expression("y"),
            env,
            "false"});
    env->define("false", false_value);
}

// Create a global environment with built-ins
env_ptr create_global_environment()
{
    auto env = std::make_shared<environment>();

    auto define_builtin = [env](const std::string& name, 
                    std::function<value_ptr(const std::vector<value_ptr>&, env_ptr)> func)
    {
        env->define(name, std::make_shared<value>(builtin_operative{name, std::move(func)}));
    };

    auto define_arithmetic = [define_builtin](const std::string& name, 
                    std::function<int(int, int)> op)
    {
        define_builtin(name, builtins::make_arithmetic_operative(name, op));
    };

    // Control
    define_builtin("vau", builtins::vau_operative);
    define_builtin("eval", builtins::eval_operative);
    define_builtin("define", builtins::define_operative);
    define_builtin("invoke", builtins::invoke_operative);
    define_builtin("try", builtins::try_operative);
#define USE_PRIMITIVE_DO
#ifdef USE_PRIMITIVE_DO
    define_builtin("do", builtins::do_operative);
#endif
    // Arithmetic
    define_arithmetic("+", std::plus<int>{});
    define_arithmetic("-", std::minus<int>{});
    define_arithmetic("*", std::multiplies<int>{});
    define_arithmetic("/", std::divides<int>{});
    // Lists
    define_builtin("cons", builtins::cons_operative);
    define_builtin("first", builtins::first_operative);
    define_builtin("rest", builtins::rest_operative);
    define_builtin("nil?", builtins::nil_p_operative);
    // Equality
    define_builtin("=", builtins::equal_operative);
    // I/O
    define_builtin("write", builtins::write_operative);
    define_builtin("display", builtins::display_operative);
    // Mutables
    define_builtin("define-mutable", builtins::define_mutable_operative);
    define_builtin("set!", builtins::set_operative);

    add_church_boleans(env);
    return env;
}

// Bind parameters to operands in target environment
void bind_parameters(const param_pattern& params, value_ptr operands, env_ptr target_env)
{
    NOEVAL_DEBUG(env_binding, "Binding parameters: {} to operands: {}", 
            params.is_variadic ? "variadic" : "fixed", value_to_string(operands));

    if (params.is_variadic) {
        // Variadic case: bind all operands to the single parameter name
        if (params.param_names.size() != 1) {
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

value_ptr operate_operative(const operative& op, value_ptr operands, env_ptr env)
{
    // Create new environment for the operative
    auto new_env = std::make_shared<environment>(op.closure_env);

    // Bind parameters to unevaluated operands
    try {
        bind_parameters(op.params, operands, new_env);
    } catch (const evaluation_error& e) {
        throw evaluation_error(std::format("{} {}", op.to_string(), e.what()), e.context, call_stack::format());
    }

    // Bind environment parameter
    NOEVAL_DEBUG(env_binding, "Binding env parameter '{}' to environment {}", 
              op.env_param, static_cast<const void*>(env.get()));
    // We use nil as the equivalent to Kernel's #ignore for vau's
    // environment parameter.
    if (not op.env_param.empty()) {
        new_env->define(op.env_param, std::make_shared<value>(env));
    }

    // Evaluate body in new environment
    return eval(op.body, new_env);
}

value_ptr operate_builtin(const builtin_operative& op, value_ptr operands, env_ptr env)
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

#if 1
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
#else
value_ptr eval_symbol(const symbol& sym, env_ptr env)
{
    // Look up the symbol in the environment
    try {
        return env->lookup(sym.name);
    } catch (const std::exception& e) {
        throw evaluation_error(e.what(), sym.name, call_stack::format());
    }
}
#endif

value_ptr eval_operation(const cons_cell& cell, env_ptr env)
{
    // Convert cons_cell back to value_ptr for easier handling
    auto expr = std::make_shared<value>(cell);
    
    if (is_nil(expr)) {
        throw evaluation_error("Cannot evaluate empty list", "()", call_stack::format());
    }
    
    auto operator_expr = car(expr);
    auto operands = cdr(expr);
    
    // Check if operator is already an operative value
    value_ptr op;
    if (std::holds_alternative<operative>(operator_expr->data) || 
        std::holds_alternative<builtin_operative>(operator_expr->data)) {
        // Use the operative directly
        op = operator_expr;
    } else {
        // Evaluate the operator expression
        op = eval(operator_expr, env);
    }
    
    // Check if it's an operative
    if (std::holds_alternative<operative>(op->data)) {
        return operate_operative(std::get<operative>(op->data), operands, env);
    }

    // Check if it's a builtin operative
    if (std::holds_alternative<builtin_operative>(op->data)) {
        return operate_builtin(std::get<builtin_operative>(op->data), operands, env);
    }

    throw evaluation_error(
        std::format("Not an operative: {}", value_to_string(op)),
        expr_context(expr),
        call_stack::format()
    );
}

value_ptr eval(value_ptr expr, env_ptr env)
{
    call_stack::guard g(expr);
    NOEVAL_DEBUG(eval, "{}[{}] Evaluating({}): {}", 
        call_stack::indent(), 
        call_stack::depth(),
        value_type_string(expr),
        value_to_string(expr));
    try {
        value_ptr result = std::visit([&](const auto& v) -> value_ptr {
            using T = std::decay_t<decltype(v)>;
            
            if constexpr (std::is_same_v<T, int> || 
                         std::is_same_v<T, std::string> || 
                         std::is_same_v<T, std::nullptr_t>) {
                return expr;
            } else if constexpr (std::is_same_v<T, symbol>) {
                return eval_symbol(v, env);
            } else if constexpr (std::is_same_v<T, cons_cell>) {
                return eval_operation(v, env);
            } else {
                throw evaluation_error(
                    std::format("Cannot evaluate {}", demangle<T>()),
                    expr_context(expr),
                    call_stack::format()
                );
            }
        }, expr->data);
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

bool load_library_file(const std::string& filename, env_ptr env)
{
    bool ok{true};
    try {
        std::string content = read_file_content(filename);
        
        if (content.empty()) {
            return ok;
        }
        
        std::println("Loading library: {}", filename);
        
        parser p(content);
        auto expressions = p.parse_all();
        
        for (const auto& expr : expressions) {
            try {
                auto result = eval(expr, env);
                NOEVAL_DEBUG(library, "Loaded: {} => {}", value_to_string(expr), value_to_string(result));
            } catch (const std::exception& e) {
                ok = false;
                std::println("  Error loading expression '{}': {}", value_to_string(expr), e.what());
            }
        }
        
        if (ok) std::println("Library loaded successfully.\n");
        
    } catch (const std::exception& e) {
        std::println("Warning: Could not load library {}: {}", filename, e.what());
        ok = false;
    }
    return ok;
}

// Function to run library tests from file
int run_library_tests(env_ptr outer_env)
{
    std::println("Running library tests from file...");
    
    try {
        // Load and run the test file
        std::string content = read_file_content("src/tests.noeval");
        
        if (content.empty()) {
            std::println("Test file is empty or not found");
            return 1;
        }
        
        parser p(content);
        auto expressions = p.parse_all();
        
        // Make an isolated test environment
        auto env = std::make_shared<environment>(outer_env);

        value_ptr result;
        for (const auto& expr : expressions) {
            try {
                result = eval(expr, env);
            } catch (const std::exception& e) {
                std::println("Error in test: {}", e.what());
                return 1;
            }
        }
        
        // The last expression should be the test result
        if (result) {
            std::string result_str = value_to_string(result);
            if (result_str == "\"All library tests passed!\"") {
                std::println("✓ {}", result_str.substr(1, result_str.length() - 2)); // Remove quotes
                return 0;
            } else {
                println_red("✗ Library tests failed with result: {}", result_str);
                return 1;
            }
        }
        
        println_red("✗ No test result returned");
        return 1;
        
    } catch (const std::exception& e) {
        println_red("✗ Failed to run library tests: {}", e.what());
        return 1;
    }
}

env_ptr reload_global_environment(bool run_tests)
{
    // Create global environment and load library
    auto global_env = create_global_environment();
    global_env->define("env", std::make_shared<value>(global_env));
    
    // Load standard library
    std::println("Loading standard library...");
    bool library_ok = load_library_file("src/lib.noeval", global_env);
    if (not library_ok) {
        std::println("Loading the library failed!");
        return nullptr;
    }

    if (run_tests) {
        int failures{0};
        // Run library tests after loading
        std::println("\n{}", std::string(60, '='));
        std::println("Running library tests...");
        failures += run_library_tests(global_env);
        std::println("{}", std::string(60, '='));
        if (failures != 0) {
            println_red("\n✗ {} test(s) failed in library tests!", failures);
            return nullptr;
        }
        std::println("\n✓ All tests passed!");
    }

    return global_env;
}

int main()
{
    if (!run_tests()) {
        return EXIT_FAILURE;
    }

    // Create global environment and load library
    auto global_env = reload_global_environment(true);
    if (not global_env) return EXIT_FAILURE;

    std::println("Starting REPL...");
    repl(global_env);

#if 0
    std::println("---");
    std::println("move_only_function support: {}", 
                  __cpp_lib_move_only_function >= 202110L ? "available" : "not available");
    std::println("copyable_function support: {}", 
                  __cpp_lib_copyable_function >= 202306L ? "available" : "not available");
    std::println("function_ref support: {}", 
                  __cpp_lib_function_ref >= 202306L ? "available" : "not available");
#endif
    return EXIT_SUCCESS;
}
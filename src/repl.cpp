#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <print>
#include <ranges>
#include <sstream>
#include <string>

#include <readline/history.h>
#include <readline/readline.h>

#include "debug.hpp"
#include "noeval.hpp"
#include "parser.hpp"
#include "repl.hpp"

std::string get_history_file()
{
    const char* home = std::getenv("HOME");
    if (!home) {
        throw std::runtime_error("HOME environment variable not set");
    }
    return std::format("{}/.noeval_history", home);
}

std::string read_with_readline(const std::string& prompt)
{
    std::unique_ptr<char, decltype([](char* p){ std::free(p); })>
        line(readline(prompt.c_str()));
    if (not line) return ""; // Handle EOF
    return line.get();
}

// Print welcome message
void print_welcome()
{
    std::println("Welcome to the Noeval Language REPL!");
    std::println("Type expressions to evaluate them, or 'quit' to exit.");
    std::println("Multi-line expressions are supported - just keep typing!");
    std::println("Special commands start with ':' (try ':help')");
    std::println("Examples:");
    std::println("  42");
    std::println("  (+ 1 2 3)");
    std::println("  (define x 10)");
    std::println("  (vau (name) env (eval name env))");
    std::println("  :debug on eval");
    std::println("");
}

// Check if an expression is syntactically complete
bool is_complete_expression(const std::string& input)
{
    int paren_count = 0;
    bool in_string = false;
    bool escaped = false;
    
    for (char ch : input) {
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (in_string) {
            if (ch == '\\') {
                escaped = true;
            } else if (ch == '"') {
                in_string = false;
            }
        } else {
            switch (ch) {
                case '"':
                    in_string = true;
                    break;
                case '(':
                    paren_count++;
                    break;
                case ')':
                    paren_count--;
                    if (paren_count < 0) {
                        // Too many closing parens - let parser handle the error
                        return true;
                    }
                    break;
            }
        }
    }
    
    // Complete if:
    // 1. Not in a string
    // 2. Parentheses are balanced
    // 3. We have at least some non-whitespace content
    return !in_string && paren_count == 0;
}

// Read a complete expression from the user, handling multi-line input
std::string read_expression()
{
    std::string input;
    std::string accumulated_input;
    
    while (true) {
        // Show different prompt for continuation lines
        input = read_with_readline(
            accumulated_input.empty()? "noeval> ": "...> ");

        // Check for EOF
        if (input.empty() and accumulated_input.empty()) return "";
        
        // Add the new line to accumulated input
        if (!accumulated_input.empty()) {
            accumulated_input += " ";  // Add space between lines
        }
        accumulated_input += input;
        
        // Trim whitespace
        std::string trimmed = accumulated_input;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
        
        if (trimmed.empty()) {
            accumulated_input.clear();
            continue;
        }
        
        // Check for special commands
        if (trimmed == "quit" || trimmed == "exit") {
            return trimmed;
        }
        
        // Check if we have a complete expression
        if (is_complete_expression(trimmed)) {
            if (not trimmed.empty()) add_history(trimmed.c_str());
            return trimmed;
        }
        // If incomplete, continue the loop to read more input
    }
}

// Check if input is a quit command
bool is_quit_command(const std::string& input)
{
    return input == "quit" || input == "exit";
}

// Helper function to handle debug commands
bool handle_debug_command(const std::string& input)
{
    if (!input.starts_with(":debug")) {
        return false; // Not a debug command
    }
    
    std::istringstream iss(input);
    std::string command, action, category;
    iss >> command >> action;
    
    if (action == "help" || action.empty()) {
        std::println("Debug commands:");
        std::println("  :debug on [category]    - Enable debug output (all categories if none specified)");
        std::println("  :debug off [category]   - Disable debug output (all categories if none specified)");
        std::println("  :debug status           - Show current debug settings");
        std::println("  :debug colors on/off    - Enable/disable colored output");
        std::println("");
        std::println("Categories: {}", debug_categories | std::views::keys);
        return true;
    }
    
    if (action == "status") {
        std::println("Debug status:");
        std::println("  Colors: {}", get_debug().are_colors_enabled()? "enabled": "disabled");
        std::println("  Enabled categories:");
        
        auto enabled = get_debug().get_enabled_categories() | std::ranges::to<std::vector>();
        
        if (enabled.empty()) {
            std::println("    (none)");
        } else {
            for (const auto& cat : enabled) {
                std::println("    {}", cat);
            }
        }
        return true;
    }
    
    if (action == "colors") {
        iss >> category; // Actually the on/off value
        if (category == "on") {
            get_debug().set_colors(true);
            std::println("Debug colors enabled");
        } else if (category == "off") {
            get_debug().set_colors(false);
            std::println("Debug colors disabled");
        } else {
            std::println("Usage: :debug colors on|off");
        }
        return true;
    }
    
    if (action == "on") {
        iss >> category;
        if (category.empty()) {
            get_debug().enable_all();
            std::println("All debug output enabled");
        } else {
            try {
                get_debug().enable(category);
                std::println("Debug category '{}' enabled", category);
            } catch (const std::exception& e) {
                std::println("Error: {}", e.what());
            }
        }
        return true;
    }
    
    if (action == "off") {
        iss >> category;
        if (category.empty()) {
            get_debug().disable_all();
            std::println("All debug output disabled");
        } else {
            try {
                get_debug().disable(category);
                std::println("Debug category '{}' disabled", category);
            } catch (const std::exception& e) {
                std::println("Error: {}", e.what());
            }
        }
        return true;
    }
    
    std::println("Unknown debug action: {}. Try ':debug help'", action);
    return true;
}

// Check if input is a special command
bool is_special_command(const std::string& input)
{
    return input.starts_with(":") || input == "quit" || input == "exit";
}

// Handle special commands (returns true if command was handled)
bool handle_special_command(const std::string& input)
{
    if (handle_debug_command(input)) {
        return true;
    }
    
    if (input == ":help") {
        std::println("Special commands:");
        std::println("  :help          - Show this help");
        std::println("  :reload        - Recreate the global environment and reload the library");
        std::println("  :debug ...     - Debug control commands (:debug help for details)");
        std::println("  quit, exit     - Exit the REPL");
        std::println("");
        std::println("Or enter any Noeval expression to evaluate it.");
        return true;
    }
    
    return false; // Not handled
}

// Evaluate an expression string in the given environment
value_ptr eval_expression(const std::string& expr_str, env_ptr env)
{
    parser p(expr_str);
    auto expr = p.parse();
    return eval(expr, env);
}

// Print the result of evaluation
void print_result(value_ptr result)
{
    std::println("=> {}", value_to_string(result));
}

// Print an error message
void print_error(const std::exception& e)
{
    std::println("Error: {}", e.what());
}

// Simple REPL with multi-line support
void repl(env_ptr global_env)
{
#if 0
    // I'm not ready to enable saving the history yet.
    auto history_file = get_history_file();
    read_history(history_file.c_str());
#endif

    print_welcome();
    
    while (true) {
        std::string input = read_expression();
        
        // Check for EOF or quit commands
        if (input.empty()) {
            std::println("\nGoodbye!");
            break;
        }
        
        // Handle special commands first
        if (is_special_command(input)) {
            // Handle reload command specially
            if (input == ":reload") {
                global_env = reload_global_environment();
                continue;
            }

            if (handle_special_command(input)) {
                continue; // Command was handled
            }
            
            // Check for quit commands
            if (is_quit_command(input)) {
                std::println("Goodbye!");
                break;
            }
        }
        
        try {
            auto result = eval_expression(input, global_env);
            print_result(result);
        }
        catch (const std::exception& e) {
            print_error(e);
        }
    }
#if 0
    write_history(history_file.c_str());
#endif
}

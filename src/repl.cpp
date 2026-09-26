#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <sstream>
#include <string>

#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#include "debug.hpp"
#include "noeval.hpp"
#include "parser.hpp"
#include "repl.hpp"

std::string get_history_file()
{
    const char* home = std::getenv("HOME");
    if (not home) {
        throw std::runtime_error("HOME environment variable not set");
    }
    return std::format("{}/.noeval_history", home);
}

// Returns std::nullopt at EOF
std::optional<std::string> read_with_readline(const std::string& prompt)
{
    std::unique_ptr<char, decltype([](char* p){ std::free(p); })>
        line(readline(prompt.c_str()));
    if (not line) return std::nullopt;
    return line.get();
}

// The environment the REPL is evaluating in, used for tab completion
static std::weak_ptr<environment> completion_env;

// Whether readline is reading the first line of an expression, which is the
// only place special commands are recognized
static bool reading_first_line{true};

// The candidates the completion generator filters by the text being completed
static std::vector<std::string> completion_candidates;

// Completion generator function
char* completion_generator(const char* prefix, int state)
{
    static std::vector<std::string> matches;
    static size_t match_index{0};

    if (0 == state) {
        // First call - generate all matches
        matches.clear();
        match_index = 0;
        
        // So annoying that we have std::bind_back, but it doesn't work
        // with overload sets.
        auto string_starts_with = [](const std::string& str, const char* prefix) {
            return std::string_view(str).starts_with(prefix);
        };
        std::ranges::copy(
            completion_candidates
                | std::views::filter(std::bind_back(string_starts_with, prefix)),
            std::back_inserter(matches));

        std::ranges::sort(matches);
        const auto [first, last] = std::ranges::unique(matches);
        matches.erase(first, last);
    }
    
    if (match_index < matches.size()) {
        return strdup(matches[match_index++].c_str());
    }
    
    return nullptr;
}

// Candidates for completing a word of a special command, given the words
// that precede it
std::vector<std::string> special_command_candidates(
    const std::vector<std::string>& words)
{
    if (words.empty()) return {":help", ":reload", ":debug"};
    if (std::vector<std::string>{":reload"} == words) return {"fast"};
    if (std::vector<std::string>{":debug"} == words) {
        return {"help", "status", "colors", "on", "off", "env-counts"};
    }
    if (2 == words.size() and ":debug" == words[0]) {
        if ("colors" == words[1]) return {"on", "off"};
        if ("on" == words[1] or "off" == words[1]) {
            return debug_categories | std::views::keys | std::ranges::to<std::vector>();
        }
    }
    return {};
}

// Completion function
char** symbol_completion(const char* text, int start, int)
{
    // Don't complete filenames
    rl_attempted_completion_over = 1;

    // Special commands get their own completions
    std::string_view before{rl_line_buffer, static_cast<size_t>(start)};
    auto words = before
        | std::views::split(' ')
        | std::views::filter([](auto word){ return not std::ranges::empty(word); })
        | std::ranges::to<std::vector<std::string>>();
    bool first_word = words.empty();
    bool special_command = reading_first_line
        and (first_word? ':' == text[0]: words[0].starts_with(':'));
    if (special_command) {
        completion_candidates = special_command_candidates(words);
        return rl_completion_matches(text, completion_generator);
    }
    
    // Only complete at word boundaries or after certain characters
    if (0 == start or strchr("( \t\n", rl_line_buffer[start - 1])) {
        // Get all symbols visible from the REPL's environment
        completion_candidates.clear();
        if (auto env = completion_env.lock()) {
            completion_candidates = env->get_all_symbols();
        }
        if (reading_first_line and first_word) {
            completion_candidates.push_back("quit");
            completion_candidates.push_back("exit");
        }
        return rl_completion_matches(text, completion_generator);
    }
    
    return nullptr;
}

// Initialize tab completion
void setup_completion()
{
    rl_attempted_completion_function = symbol_completion;
    rl_completer_word_break_characters = " \t\n()";
}

// Readline's getc function when stdin isn't a terminal. It reads through
// std::cin's stream buffer, the same one `read` parses from, so the REPL and
// `read` take characters from the input in order. (Readline's own getc reads
// the file descriptor directly, missing whatever stdio or `read`'s parser has
// already buffered.)
int getc_from_cin(FILE*)
{
    using traits = std::char_traits<char>;
    while (true) {
        auto ch = std::cin.rdbuf()->sbumpc();
        if (not traits::eq_int_type(ch, traits::eof())) {
            return static_cast<unsigned char>(traits::to_char_type(ch));
        }
        // Retry if a signal interrupted the read rather than the input ending
        if (not (std::ferror(stdin) and EINTR == errno)) return EOF;
        std::clearerr(stdin);
    }
}

// Have readline share std::cin's input when stdin isn't a terminal. On a
// terminal, input arrives a line at a time, so nothing gets buffered past
// what's been asked for, and readline needs its own getc for key sequences.
void setup_input()
{
    if (not isatty(STDIN_FILENO)) rl_getc_function = getc_from_cin;
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
    
    for (char ch: input) {
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (in_string) {
            if ('\\' == ch) {
                escaped = true;
            } else if ('"' == ch) {
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
    return not in_string and 0 == paren_count;
}

// Read a complete expression from the user, handling multi-line input
// Returns std::nullopt at EOF, discarding any incomplete expression
std::optional<std::string> read_expression()
{
    std::string accumulated_input;
    
    while (true) {
        // Show different prompt for continuation lines
        reading_first_line = accumulated_input.empty();
        auto input = read_with_readline(
            accumulated_input.empty()? "noeval> ": "...> ");

        if (not input) return std::nullopt;
        
        // Add the new line to accumulated input
        if (not accumulated_input.empty()) {
            accumulated_input += " ";  // Add space between lines
        }
        accumulated_input += *input;
        
        // Trim whitespace
        std::string trimmed = accumulated_input;
        trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
        trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
        
        if (trimmed.empty()) {
            accumulated_input.clear();
            continue;
        }
        
        // Check for special commands
        if ("quit" == trimmed or "exit" == trimmed) {
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
    return "quit" == input or "exit" == input;
}

// Helper function to handle debug commands
bool handle_debug_command(const std::string& input)
{
    if (not input.starts_with(":debug")) {
        return false; // Not a debug command
    }
    
    std::istringstream iss(input);
    std::string command, action, category;
    iss >> command >> action;

    if ("help" == action or action.empty()) {
        std::println("Debug commands:");
        std::println("  :debug on [category]    - Enable debug output (all categories if none specified)");
        std::println("  :debug off [category]   - Disable debug output (all categories if none specified)");
        std::println("  :debug status           - Show current debug settings");
        std::println("  :debug colors on/off    - Enable/disable colored output");
        std::println("  :debug on stack-depth   - Show max stack depth after each evaluation");
        std::println("  :debug on gc            - Show garbage collection info");
        std::println("  :debug env-counts       - Show environment construction and registration counts");
        std::println("");
        auto categories{debug_categories | std::views::keys | std::ranges::to<std::vector>()};
        std::ranges::sort(categories);
        std::println("Categories: {}",
            categories | std::views::join_with(std::string_view{", "}) | std::ranges::to<std::string>());
        return true;
    }

    if ("status" == action) {
        std::println("Debug status:");
        std::println("  Colors: {}", get_debug().are_colors_enabled()? "enabled": "disabled");
        std::println("  Enabled categories:");
        
        auto enabled = get_debug().get_enabled_categories() | std::ranges::to<std::vector>();
        
        if (enabled.empty()) {
            std::println("    (none)");
        } else {
            for (const auto& cat: enabled) {
                std::println("    {}", cat);
            }
        }
        return true;
    }

    if ("colors" == action) {
        iss >> category; // Actually the on/off value
        if ("on" == category) {
            get_debug().set_colors(true);
            std::println("Debug colors enabled");
        } else if ("off" == category) {
            get_debug().set_colors(false);
            std::println("Debug colors disabled");
        } else {
            std::println("Usage: :debug colors on|off");
        }
        return true;
    }

    if ("on" == action) {
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

    if ("off" == action) {
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

    if ("env-counts" == action) {
        std::println("Environment counts:");
        std::println("  Constructed: {}", environment::get_constructed_count());
        std::println("  Registered:  {}", environment::get_registered_count());
        return true;
    }

    std::println("Unknown debug action: {}. Try ':debug help'", action);
    return true;
}

// Check if input is a special command
bool is_special_command(const std::string& input)
{
    return input.starts_with(":") or "quit" == input or "exit" == input;
}

// Handle special commands (returns true if command was handled)
bool handle_special_command(const std::string& input)
{
    if (handle_debug_command(input)) {
        return true;
    }

    if (":help" == input) {
        std::println("Special commands:");
        std::println("  :help          - Show this help");
        std::println("  :reload        - Recreate the global environment and reload the library (with tests)");
        std::println("  :reload fast   - Recreate the global environment and reload the library (skip tests)");
        std::println("  (redefine name value) - Like define, but may rebind a name (REPL only)");
        std::println("  :debug ...     - Debug control commands (:debug help for details)");
        std::println("  quit, exit     - Exit the REPL");
        std::println("");
        std::println("Or enter any Noeval expression to evaluate it.");
        return true;
    }
    
    if (input.starts_with(":reload")) {
        std::istringstream iss(input);
        std::string command, option;
        iss >> command >> option;
        
        bool test_the_library = ("fast" != option);
        bool ok = static_cast<bool>(reload_top_level_environment(test_the_library));
        if (ok) {
            std::println("Environment reloaded successfully{}", 
                        test_the_library? " (with tests)": " (skipping tests)");
        } else {
            std::println("Failed to reload environment");
        }
        return true;
    }
    
    return false; // Not handled
}

// Evaluate an expression string in the given environment
value_ptr eval_expression(const std::string& expr_str, env_ptr env)
{
    parser p(expr_str);
    auto expr = p.parse();
    return top_level_eval(expr, env);
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
void repl(env_ptr env)
{
    // Only the REPL may rebind a name, with redefine
    add_repl_bindings(env);
    completion_env = env;
#if 0
    // I'm not ready to enable saving the history yet.
    auto history_file = get_history_file();
    read_history(history_file.c_str());
#endif
    setup_completion();
    setup_input();

    print_welcome();
    
    while (true) {
        auto maybe_input = read_expression();
        
        // Treat EOF like a quit command
        if (not maybe_input) {
            // Readline ends the prompt line itself only on a terminal
            if (not isatty(STDIN_FILENO)) std::println("");
            std::println("Goodbye!");
            break;
        }
        std::string input = *maybe_input;
        
        // Handle special commands first
        if (is_special_command(input)) {
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
            call_stack_reset_max_depth();
            auto result = eval_expression(input, env);
            NOEVAL_DEBUG(stack-depth, "max stack depth: {}", call_stack_get_max_depth());
            print_result(result);
        } catch (const std::exception& e) {
            print_error(e);
        }
    }
#if 0
    write_history(history_file.c_str());
#endif
}

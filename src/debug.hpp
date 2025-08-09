#pragma once

#include <format>
#include <print>
#include <string>
#include <unordered_map>
#include <unordered_set>

#define NOEVAL_DEBUG(category, ...) get_debug().log(#category, __VA_ARGS__)
#define NOEVAL_DEBUG_ENABLED(category) get_debug().is_enabled(#category)

extern std::unordered_map<std::string, std::string> debug_categories;

class debug_controller {
private:
    std::unordered_set<std::string> enabled_categories;
    bool use_colors = true;
    
public:
    void enable(const std::string& category);
    void disable(const std::string& category);
    std::unordered_set<std::string> get_enabled_categories() const;
    void set_enabled_categories(const std::unordered_set<std::string>& categories);
    void enable_all();
    void disable_all();
    bool is_enabled(const std::string& category) const;
    void set_colors(bool enable);
    bool are_colors_enabled() const;
    
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
    std::string get_prefix(const std::string& category) const;
    std::string get_color(const std::string& category) const;
};

debug_controller& get_debug();

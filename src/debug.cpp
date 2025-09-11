#include <format>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "debug.hpp"

// Currently, the only data here is the color.
// But this also defines what choices there are.
std::unordered_map<std::string, std::string> debug_categories{
    {"eval", "\033[36m"},        // Cyan
    {"env_lookup", "\033[32m"},  // Green
    {"env_binding", "\033[33m"}, // Yellow
    {"env_dump", "\033[35m"},    // Magenta
    {"operative", "\033[34m"},   // Blue
    {"builtin", "\033[90m"},     // Dark gray
    {"parse", "\033[31m"},       // Red
    {"library", "\033[95m"},     // Light magenta
    {"error", "\033[91m"},       // Light red
    {"stack-depth", "\033[0m"},  // Reset
    {"gc", "\033[0m"},           // Reset
    {"tco", "\033[0m"},          // Reset
    {"timer", "\033[0m"},        // Reset
    {"gc_roots", "\033[0m"},     // Reset
    {"all", "\033[0m"},          // Reset
    {"none", "\033[0m"},         // Reset
};

// Global debug controller
debug_controller& get_debug()
{
    static debug_controller instance;
    return instance;
}

void debug_controller::enable(const std::string& category)
{
    if (not debug_categories.contains(category)) {
        throw std::runtime_error("Unknown debug category: " + category);
    }
    enabled_categories.insert(category);
}

void debug_controller::disable(const std::string& category)
{ enabled_categories.erase(category); }

std::unordered_set<std::string> debug_controller::get_enabled_categories() const
{ return enabled_categories; }

void debug_controller::set_enabled_categories(const std::unordered_set<std::string>& categories)
{ enabled_categories = categories; }

void debug_controller::enable_all()
{ enabled_categories = std::views::keys(debug_categories) | std::ranges::to<std::unordered_set>(); }

void debug_controller::disable_all()
{ enabled_categories.clear(); }

bool debug_controller::is_enabled(const std::string& category) const
{ return enabled_categories.contains(category); }

void debug_controller::set_colors(bool enable) { use_colors = enable; }
bool debug_controller::are_colors_enabled() const { return use_colors; }

// The parameter cannot (yet) be a string_view because unordered_map
// doesn't (yet) support heterogeneous lookups.
std::string debug_controller::get_prefix(const std::string& category) const
{
    if (not debug_categories.contains(category)) {
        throw std::runtime_error("Unknown debug category: " + category);
    }
    //TODO: Do we want to make this uppercase?
    return std::format("[{}]", category);
}

std::string debug_controller::get_color(const std::string& category) const
{
    return debug_categories.at(category);
}

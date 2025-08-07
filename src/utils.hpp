#pragma once

#include <format>
#include <print>
#include <string>
#include <string_view>
#include <typeinfo>

std::string demangle(std::type_info const& type);

template <typename T>
std::string demangle()
{
    return demangle(typeid(T));
}

template <typename T> std::string demangle();
void println_red(std::string_view format_str, auto&&... args)
{
    std::println("\033[31m{}\033[0m",
        std::vformat(format_str,
            std::make_format_args(args...)));
}

// Helper function to read file content
std::string read_file_content(const std::string& filename);

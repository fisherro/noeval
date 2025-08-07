#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>

#include <cxxabi.h>

#include "utils.hpp"

std::string demangle(std::type_info const& type)
{
    int status = 0;
    char* name = abi::__cxa_demangle(type.name(), nullptr, nullptr, &status);
    std::string result(name? name: type.name());
    std::free(name);
    return result;
}

// Helper function to read file content
std::string read_file_content(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open library file: " + filename);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
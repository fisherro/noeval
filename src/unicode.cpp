#include "unicode.hpp"
#include <stdexcept>
#include <format>

std::u8string utf32_to_utf8(std::u32string_view utf32)
{
    std::u8string result;
    result.reserve(utf32.size() * 2); // Reasonable estimate for most text
    
    for (char32_t codepoint: utf32) {
        // Validate Unicode codepoint range
        if (codepoint > 0x10FFFF) {
            throw std::invalid_argument(
                std::format("Invalid Unicode codepoint: U+{:X} (must be <= U+10FFFF)", 
                           static_cast<uint32_t>(codepoint)));
        }
        
        // Check for surrogate pairs (invalid in Unicode strings)
        if (codepoint >= 0xD800 and codepoint <= 0xDFFF) {
            throw std::invalid_argument(
                std::format("Invalid Unicode codepoint: U+{:X} (surrogate pair range not allowed)", 
                           static_cast<uint32_t>(codepoint)));
        }
        
        // Encode to UTF-8
        if (codepoint <= 0x7F) {
            // 1-byte sequence: 0xxxxxxx
            result.push_back(static_cast<char8_t>(codepoint));
        } else if (codepoint <= 0x7FF) {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            result.push_back(static_cast<char8_t>(0xC0 | (codepoint >> 6)));
            result.push_back(static_cast<char8_t>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            result.push_back(static_cast<char8_t>(0xE0 | (codepoint >> 12)));
            result.push_back(static_cast<char8_t>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char8_t>(0x80 | (codepoint & 0x3F)));
        } else {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            result.push_back(static_cast<char8_t>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char8_t>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char8_t>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char8_t>(0x80 | (codepoint & 0x3F)));
        }
    }
    
    return result;
}

std::u32string utf8_to_utf32(std::u8string_view utf8)
{
    std::u32string result;
    result.reserve(utf8.size()); // Overestimate, but reasonable
    
    for (size_t i = 0; i < utf8.size(); ) {
        char8_t first_byte = utf8[i];
        char32_t codepoint;
        size_t sequence_length;

        if (0 == (first_byte & 0x80)) {
            // 1-byte sequence: 0xxxxxxx
            codepoint = first_byte;
            sequence_length = 1;
        } else if (0xC0 == (first_byte & 0xE0)) {
            // 2-byte sequence: 110xxxxx 10xxxxxx
            if (i + 1 >= utf8.size()) {
                throw std::invalid_argument("Truncated UTF-8 sequence");
            }
            
            char8_t second_byte = utf8[i + 1];
            if (0x80 != (second_byte & 0xC0)) {
                throw std::invalid_argument(
                    std::format("Invalid UTF-8 continuation byte: 0x{:02X}", 
                               static_cast<uint8_t>(second_byte)));
            }
            
            codepoint = ((first_byte & 0x1F) << 6) | (second_byte & 0x3F);
            sequence_length = 2;
            
            // Check for overlong encoding
            if (codepoint < 0x80) {
                throw std::invalid_argument("Overlong UTF-8 encoding");
            }
        } else if (0xE0 == (first_byte & 0xF0)) {
            // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
            if (i + 2 >= utf8.size()) {
                throw std::invalid_argument("Truncated UTF-8 sequence");
            }
            
            char8_t second_byte = utf8[i + 1];
            char8_t third_byte  = utf8[i + 2];
            
            if (0x80 != (second_byte & 0xC0) or 0x80 != (third_byte & 0xC0)) {
                throw std::invalid_argument("Invalid UTF-8 continuation bytes");
            }
            
            codepoint = ((first_byte & 0x0F) << 12) | 
                        ((second_byte & 0x3F) << 6) | 
                         (third_byte & 0x3F);
            sequence_length = 3;
            
            // Check for overlong encoding and surrogate pairs
            if (codepoint < 0x800) {
                throw std::invalid_argument("Overlong UTF-8 encoding");
            }
            if (codepoint >= 0xD800 and codepoint <= 0xDFFF) {
                throw std::invalid_argument(
                    std::format("UTF-8 encoded surrogate pair: U+{:X}", 
                               static_cast<uint32_t>(codepoint)));
            }
        } else if (0xF0 == (first_byte & 0xF8)) {
            // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            if (i + 3 >= utf8.size()) {
                throw std::invalid_argument("Truncated UTF-8 sequence");
            }
            
            char8_t second_byte = utf8[i + 1];
            char8_t third_byte  = utf8[i + 2];
            char8_t fourth_byte = utf8[i + 3];
            
            if (0x80 != (second_byte & 0xC0) or 
                0x80 != (third_byte & 0xC0)  or 
                0x80 != (fourth_byte & 0xC0)) {
                throw std::invalid_argument("Invalid UTF-8 continuation bytes");
            }
            
            codepoint = ((first_byte & 0x07) << 18)  | 
                        ((second_byte & 0x3F) << 12) |
                        ((third_byte & 0x3F) << 6)   | 
                        (fourth_byte & 0x3F);
            sequence_length = 4;
            
            // Check for overlong encoding and valid Unicode range
            if (codepoint < 0x10000) {
                throw std::invalid_argument("Overlong UTF-8 encoding");
            }
            if (codepoint > 0x10FFFF) {
                throw std::invalid_argument(
                    std::format("Codepoint outside Unicode range: U+{:X}", 
                               static_cast<uint32_t>(codepoint)));
            }
        } else {
            throw std::invalid_argument(
                std::format("Invalid UTF-8 start byte: 0x{:02X}", 
                           static_cast<uint8_t>(first_byte)));
        }
        
        result.push_back(codepoint);
        i += sequence_length;
    }
    
    return result;
}

#pragma once
#include <string>

// Throws an exception if codepoint isn't a valid Unicode codepoint.
std::u8string utf32_to_utf8(std::u32string_view utf32);

// Throws an exception if utf8 is invalid.
std::u32string utf8_to_utf32(std::u8string_view utf8);

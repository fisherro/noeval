#pragma once

#include <istream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "noeval.hpp"
#include "pushback_streambuf.hpp"

// Token types for lexical analysis
enum class token_type {
    left_paren,
    right_paren,
    symbol,
    number,
    string_literal,
    eof
};

std::string token_type_to_string(token_type type);

struct position {
private:
    size_t line_;
    size_t column_;
    size_t offset_;

public:
    position(size_t line = 1, size_t column = 1, size_t offset = 0) 
        : line_(line), column_(column), offset_(offset) {}
    
    // Accessors
    size_t line() const { return line_; }
    size_t column() const { return column_; }
    size_t offset() const { return offset_; }
    
    // Advance position by one character
    void advance(char ch) {
        if (ch == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        offset_++;
    }
    
    std::string to_string() const { 
        return std::format("{}:{}", line_, column_); 
    }
};

struct token {
    token_type type;
    std::string value;
    position pos;
    
    token(token_type t, std::string v = "", position p = position{});
    std::string to_string() const;
};

class lexer {
public:
    explicit lexer(std::string text);
    // The lexer reads through `in`'s stream buffer, putting its own pushback
    // buffer in its place until the lexer is destroyed. `in` must outlive the
    // lexer.
    explicit lexer(std::istream& in);
    ~lexer();
    token next_token();
    position get_position() const { return current_pos_; }

private:
    using traits = std::char_traits<char>;

    // Holds the text when lexing a string
    std::unique_ptr<std::istringstream> owned_input_;
    std::istream& in_;
    pushback_streambuf buf_;
    position current_pos_;
    
    void install_buffer();

    // Get current character (or '\0' if at end)
    char current_char() { return to_char(buf_.sgetc()); }
    
    // Check if at end of input
    bool at_end() { return traits::eq_int_type(buf_.sgetc(), traits::eof()); }
    
    // Advance position by one character
    void advance() {
        int ch = buf_.sbumpc();
        if (not traits::eq_int_type(ch, traits::eof())) {
            current_pos_.advance(traits::to_char_type(ch));
        }
    }
    
    // Peek at next character without advancing
    char peek(size_t ahead = 1) { return to_char(buf_.peek(ahead)); }

    static char to_char(int ch) {
        return traits::eq_int_type(ch, traits::eof()) ? '\0' : traits::to_char_type(ch);
    }
    
    bool matches_keyword(std::string_view keyword);
    void skip_disabled_block();
    void skip_whitespace_and_comments();
    std::string read_symbol();
    std::string read_string();
    std::string read_number();
    std::string read_based_number();
    std::string read_hex_digits();
    std::string read_octal_digits();
    std::string read_binary_digits();
    std::string read_arbitrary_base_digits(int base);
};

class parser {
public:
    // If a file name is given, the lists the parser reads record their
    // locations in that file, and parse errors include it.
    explicit parser(std::string input, std::string_view file = {});
    explicit parser(std::istream& in, std::string_view file = {});
    value_ptr parse_expression();
    value_ptr parse();
    std::vector<value_ptr> parse_all();

private:
    lexer lex;
    // Interned, or null if there's no file name
    const std::string* file_;
    // The lookahead token. It is fetched only when needed so that parsing an
    // expression doesn't read input beyond the end of that expression.
    std::optional<token> current_token_;
    
    const token& current_token();
    void advance();
    value_ptr parse_list();
    // An error at pos, with the file name (if any) and position prepended.
    std::runtime_error error(const position& pos, std::string_view message) const;
};

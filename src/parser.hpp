#pragma once

#include <string>
#include <vector>

#include "noeval.hpp"

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
    token next_token();
    position get_position() const { return current_pos_; }

private:
    std::string input_;
    position current_pos_;
    
    // Get current character (or '\0' if at end)
    char current_char() const {
        return current_pos_.offset() < input_.size() ? input_[current_pos_.offset()] : '\0';
    }
    
    // Check if at end of input
    bool at_end() const { return current_pos_.offset() >= input_.size(); }
    
    // Advance position by one character
    void advance() {
        if (not at_end()) {
            current_pos_.advance(input_[current_pos_.offset()]);
        }
    }
    
    // Peek at next character without advancing
    char peek(size_t ahead = 1) const {
        size_t pos = current_pos_.offset() + ahead;
        return pos < input_.size() ? input_[pos] : '\0';
    }
    
    bool matches_keyword(const std::string& keyword);
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
    explicit parser(std::string input);
    value_ptr parse_expression();
    value_ptr parse();
    std::vector<value_ptr> parse_all();

private:
    lexer lex;
    token current_token{token_type::eof};
    
    void advance();
    value_ptr parse_list();
};

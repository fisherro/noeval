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

struct token {
    token_type type;
    std::string value;
    
    token(token_type t, std::string v = "");
    std::string to_string() const;
};

class lexer {
public:
    explicit lexer(std::string text);
    token next_token();

private:
    std::string input;
    size_t pos;
    
    bool matches_keyword(const std::string& keyword);
    void skip_disabled_block();
    void skip_whitespace_and_comments();
    std::string read_symbol();
    std::string read_string();
    std::string read_number();
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

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <format>
#include <utility>
#include <vector>

#include "debug.hpp"
#include "parser.hpp"

std::string token_type_to_string(token_type type)
{
    switch (type) {
        case token_type::left_paren: return "LEFT_PAREN";
        case token_type::right_paren: return "RIGHT_PAREN";
        case token_type::symbol: return "SYMBOL";
        case token_type::integer: return "INTEGER";
        case token_type::string_literal: return "STRING_LITERAL";
        case token_type::eof: return "EOF";
        default: return "UNKNOWN";
    }
}

token::token(token_type t, std::string v): type(t), value(std::move(v)) {}

std::string token::to_string() const
{
    return std::format("Token({}, '{}')", token_type_to_string(type), value);
}

bool lexer::matches_keyword(const std::string& keyword)
{
    if (pos + keyword.length() > input.size()) return false;
    
    // Check if the keyword matches
    for (size_t i = 0; i < keyword.length(); ++i) {
        if (input[pos + i] != keyword[i]) return false;
    }
    
    // Ensure it's followed by whitespace, end of input, or newline
    size_t next_pos = pos + keyword.length();
    return next_pos >= input.size() || 
            std::isspace(input[next_pos]) || 
            input[next_pos] == '\n';
}

void lexer::skip_disabled_block()
{
    pos += 5; // skip "#skip"
    
    // Skip until #end
    while (pos + 4 <= input.size()) {
        if (pos + 4 <= input.size() && 
            input.substr(pos, 4) == "#end") {
            // Check if it's followed by whitespace or end of input
            if (pos + 4 >= input.size() || 
                std::isspace(input[pos + 4]) || 
                input[pos + 4] == '\n') {
                pos += 4; // skip "#end"
                return;
            }
        }
        ++pos;
    }
    throw std::runtime_error("Unterminated #skip block - missing #end");
}

void lexer::skip_whitespace_and_comments()
{
    while (pos < input.size()) {
        if (std::isspace(input[pos])) {
            ++pos;
        } else if (input[pos] == ';') {
            // Skip comment - everything until newline or end of input
            while (pos < input.size() && input[pos] != '\n') {
                ++pos;
            }
            // pos now points to newline or end, will be incremented in next iteration
        } else if (pos + 5 <= input.size() && input.substr(pos, 5) == "#skip") {
            // Check if #skip is followed by whitespace or end of input
            if (pos + 5 >= input.size() || std::isspace(input[pos + 5]) || input[pos + 5] == '\n') {
                skip_disabled_block();
            } else {
                break; // Not a skip directive, treat as regular token
            }
        } else {
            break;  // Found non-whitespace, non-comment character
        }
    }
}

std::string lexer::read_symbol()
{
    std::string result;
    while (pos < input.size() && 
            !std::isspace(input[pos]) && 
            input[pos] != '(' && 
            input[pos] != ')' &&
            input[pos] != ';') {  // Stop at semicolon to prevent comments in symbols
        result += input[pos++];
    }
    return result;
}

std::string lexer::read_string()
{
    std::string result;
    ++pos; // skip opening quote
    while (pos < input.size() && input[pos] != '"') {
        if (input[pos] == '\\' && pos + 1 < input.size()) {
            ++pos; // skip backslash
            switch (input[pos]) {
                case 'n' : result += '\n';       break;
                case 't' : result += '\t';       break;
                case '\\': result += '\\';       break;
                case '"' : result += '"';        break;
                case 'e' : result += '\033';     break;
                default  : result += input[pos]; break;
            }
        } else {
            result += input[pos];
        }
        ++pos;
    }
    if (pos < input.size()) ++pos; // skip closing quote
    return result;
}

lexer::lexer(std::string text) : input(std::move(text)), pos(0) {}
    
token lexer::next_token()
{
    skip_whitespace_and_comments();

    if (pos >= input.size()) {
        return token(token_type::eof);
    }
    
    char ch = input[pos];
    
    if (ch == '(') {
        ++pos;
        return token(token_type::left_paren);
    }
    
    if (ch == ')') {
        ++pos;
        return token(token_type::right_paren);
    }
    
    if (ch == '"') {
        return token(token_type::string_literal, read_string());
    }
    
    if (std::isdigit(ch) || (ch == '-' && pos + 1 < input.size() && std::isdigit(input[pos + 1]))) {
        std::string num_str = read_symbol();
        return token(token_type::integer, num_str);
    }
    
    // Everything else is a symbol
    return token(token_type::symbol, read_symbol());
}

void parser::advance()
{
    current_token = lex.next_token();
}

value_ptr parser::parse_list()
{
    // Expect '('
    if (current_token.type != token_type::left_paren) {
        throw std::runtime_error("Expected '('");
    }
    advance(); // consume '('
    
    if (current_token.type == token_type::right_paren) {
        advance(); // consume ')'
        return std::make_shared<value>(nullptr); // nil
    }
    
    // Parse elements
    std::vector<value_ptr> elements;
    while (current_token.type != token_type::right_paren && 
            current_token.type != token_type::eof) {
        elements.push_back(parse_expression());
    }
    
    if (current_token.type != token_type::right_paren) {
        throw std::runtime_error("Expected ')'");
    }
    advance(); // consume ')'
    
    // Build cons cells from right to left
    value_ptr result = std::make_shared<value>(nullptr); // nil
    for (auto it = elements.rbegin(); it != elements.rend(); ++it) {
        result = std::make_shared<value>(cons_cell{*it, result});
    }
    
    return result;
}

parser::parser(std::string input) : lex(std::move(input))
{
    advance(); // prime the pump
}

value_ptr parser::parse_expression()
{
    VAU_DEBUG(parse, "Parsing token: {}", current_token.to_string());
    switch (current_token.type) {
        case token_type::left_paren:
            VAU_DEBUG(parse, "Starting list parse");
            return parse_list();
            
        case token_type::symbol:
            {
                VAU_DEBUG(parse, "Parsing symbol: {}", current_token.value);
                auto result = std::make_shared<value>(symbol{current_token.value});
                advance();
                return result;
            }
            
        case token_type::integer:
            {
                VAU_DEBUG(parse, "Parsing integer: {}", current_token.value);
                int val = std::stoi(current_token.value);
                auto result = std::make_shared<value>(val);
                advance();
                return result;
            }
            
        case token_type::string_literal:
            {
                VAU_DEBUG(parse, "Parsing string literal: {}", current_token.value);
                auto result = std::make_shared<value>(current_token.value);
                advance();
                return result;
            }
            
        case token_type::eof:
            throw std::runtime_error("Unexpected end of input");
            
        default:
            throw std::runtime_error("Unexpected token");
    }
}

value_ptr parser::parse()
{
    return parse_expression();
}

// Parse all expressions from input
std::vector<value_ptr> parser::parse_all()
{
    std::vector<value_ptr> expressions;
    
    while (current_token.type != token_type::eof) {
        expressions.push_back(parse_expression());
    }
    
    return expressions;
}

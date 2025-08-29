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
        case token_type::number: return "NUMBER";
        case token_type::string_literal: return "STRING_LITERAL";
        case token_type::eof: return "EOF";
        default: return "UNKNOWN";
    }
}

token::token(token_type t, std::string v, position p)
: type(t), value(std::move(v)), pos(p) {}

std::string token::to_string() const
{
    return std::format("Token({}, '{}') at {}", token_type_to_string(type), value, pos.to_string());
}

#if 0
void lexer::update_position(char ch)
{
    if (ch == '\n') {
        current_pos.line++;
        current_pos.column = 1;
    } else {
        current_pos.column++;
    }
    current_pos.offset++;
}
#endif

bool lexer::matches_keyword(const std::string& keyword)
{
    if (current_pos_.offset() + keyword.length() > input_.size()) return false;
    
    // Check if the keyword matches
    for (size_t i = 0; i < keyword.length(); ++i) {
        if (input_[current_pos_.offset() + i] != keyword[i]) return false;
    }
    
    // Ensure it's followed by whitespace, end of input, or newline
    size_t next_pos = current_pos_.offset() + keyword.length();
    return next_pos >= input_.size() or 
            std::isspace(input_[next_pos]) or 
            input_[next_pos] == '\n';
}

void lexer::skip_disabled_block()
{
    // Skip "#skip"
    for (int i = 0; i < 5; ++i) {
        advance();
    }
    
    int nesting_depth = 1; // We're already inside one skip block
    
    while (not at_end() and nesting_depth > 0) {
        // Check for nested #skip
        if (current_pos_.offset() + 5 <= input_.size() and input_.substr(current_pos_.offset(), 5) == "#skip") {
            // Verify it's a proper keyword (followed by whitespace or end of input)
            if (current_pos_.offset() + 5 >= input_.size() or 
                std::isspace(input_[current_pos_.offset() + 5]) or 
                input_[current_pos_.offset() + 5] == '\n') {
                nesting_depth++;
                for (int i = 0; i < 5; ++i) {
                    advance();
                }
                continue;
            }
        }
        
        // Check for #end
        if (current_pos_.offset() + 4 <= input_.size() and input_.substr(current_pos_.offset(), 4) == "#end") {
            // Verify it's a proper keyword (followed by whitespace or end of input)
            if (current_pos_.offset() + 4 >= input_.size() or 
                std::isspace(input_[current_pos_.offset() + 4]) or 
                input_[current_pos_.offset() + 4] == '\n') {
                nesting_depth--;
                for (int i = 0; i < 4; ++i) {
                    advance();
                }
                if (0 == nesting_depth) {
                    return; // Found the matching #end
                }
                continue;
            }
        }
        
        advance();
    }
    
    throw std::runtime_error("Unterminated #skip block - missing #end");
}

void lexer::skip_whitespace_and_comments()
{
    while (not at_end()) {
        char ch = current_char();
        if (std::isspace(ch)) {
            advance();
        } else if (ch == ';') {
            // Skip comment - everything until newline or end of input
            while (not at_end() and current_char() != '\n') {
                advance();
            }
            // Will advance past newline in next iteration if present
        } else if (current_pos_.offset() + 5 <= input_.size() and input_.substr(current_pos_.offset(), 5) == "#skip") {
            // Check if #skip is followed by whitespace or end of input
            char next_ch = peek(5);
            if (next_ch == '\0' or std::isspace(next_ch) or next_ch == '\n') {
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
    while (not at_end() and 
           not std::isspace(current_char()) and 
           current_char() != '(' and 
           current_char() != ')' and
           current_char() != ';') {
        result += current_char();
        advance();
    }
    return result;
}

std::string lexer::read_string()
{
    std::string result;
    advance(); // skip opening quote
    
    while (not at_end() and current_char() != '"') {
        if (current_char() == '\\' and not at_end()) {
            advance(); // skip backslash
            if (at_end()) break; // Protect against malformed strings
            
            switch (current_char()) {
                case 'n' : result += '\n';       break;
                case 't' : result += '\t';       break;
                case '\\': result += '\\';       break;
                case '"' : result += '"';        break;
                case 'e' : result += '\033';     break;
                default  : result += current_char(); break;
            }
        } else {
            result += current_char();
        }
        advance();
    }
    
    if (not at_end()) advance(); // skip closing quote
    return result;
}

std::string lexer::read_number()
{
    std::string result;
    
    // Handle optional negative sign
    if (current_char() == '-') {
        result += current_char();
        advance();
    }
    
    // Read digits
    while (not at_end() and std::isdigit(current_char())) {
        result += current_char();
        advance();
    }
    
    // Check for fractional format (/)
    if (not at_end() and current_char() == '/') {
        result += current_char();
        advance();
        
        // Read denominator (must start with non-zero digit)
        if (at_end() or not std::isdigit(current_char()) or current_char() == '0') {
            throw std::runtime_error("Invalid fraction: denominator must start with non-zero digit");
        }
        while (not at_end() and std::isdigit(current_char())) {
            result += current_char();
            advance();
        }
        return result;
    }
    
    // Check for decimal format (.)
    if (not at_end() and current_char() == '.') {
        result += current_char();
        advance();
        
        // Read fractional part
        while (not at_end() and std::isdigit(current_char())) {
            result += current_char();
            advance();
        }
        
        // Check for repeating part in parentheses
        if (not at_end() and current_char() == '(') {
            result += current_char();
            advance();
            
            // Read repeating digits
            bool has_digits = false;
            while (not at_end() and std::isdigit(current_char())) {
                result += current_char();
                advance();
                has_digits = true;
            }
            if (not has_digits) {
                throw std::runtime_error("Invalid repeating decimal: empty parentheses");
            }
            if (at_end() or current_char() != ')') {
                throw std::runtime_error("Invalid repeating decimal: missing closing parenthesis");
            }
            result += current_char();
            advance(); // consume ')'
        }
    }
    
    return result;
}

lexer::lexer(std::string text) : input_(std::move(text)), current_pos_(1, 1, 0) {}

token lexer::next_token()
{
    skip_whitespace_and_comments();

    if (at_end()) {
        return token(token_type::eof, "", current_pos_);
    }
    
    position token_start = current_pos_;  // Remember where this token starts
    char ch = current_char();
    
    if (ch == '(') {
        advance();
        return token(token_type::left_paren, "", token_start);
    }
    
    if (ch == ')') {
        advance();
        return token(token_type::right_paren, "", token_start);
    }
    
    if (ch == '"') {
        return token(token_type::string_literal, read_string(), token_start);
    }

    if (std::isdigit(ch) or (ch == '-' and not at_end() and std::isdigit(peek()))) {
        position start_position = current_pos_;
        std::string number_str = read_number();
        
        // Validate that we're at a proper token boundary after reading the number
        if (not at_end() and 
            not std::isspace(current_char()) and 
            current_char() != '(' and 
            current_char() != ')' and 
            current_char() != ';') {
            // Not at a valid boundary - this means we have something like "-123abc"
            // Reset position and treat the whole thing as a symbol
            current_pos_ = start_position;
            return token(token_type::symbol, read_symbol(), token_start);
        }
        
        return token(token_type::number, number_str, token_start);
    }
    
    // Everything else is a symbol
    return token(token_type::symbol, read_symbol(), token_start);
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
    auto open_paren_position = current_token.pos;  // Remember where this list started
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
        if (current_token.type == token_type::eof) {
            throw std::runtime_error(std::format("Expected ')' to close list opened at line {}, but reached end of input", 
                                                open_paren_position.line()));
        } else {
            throw std::runtime_error(std::format("Expected ')' to close list opened at {} but found {} ('{}') at {}", 
                                                open_paren_position.to_string(),
                                                token_type_to_string(current_token.type), 
                                                current_token.value,
                                                current_token.pos.to_string()));
        }
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

bignum parse_number_string(const std::string& num_str)
{
    using cpp_int = boost::multiprecision::cpp_int;
    
    // Check if it's a fractional format (contains '/')
    size_t slash_pos = num_str.find('/');
    if (slash_pos != std::string::npos) {
        // Parse as fraction: numerator/denominator
        std::string num_part = num_str.substr(0, slash_pos);
        std::string den_part = num_str.substr(slash_pos + 1);
        
        cpp_int numerator(num_part);
        cpp_int denominator(den_part);
        
        return bignum(numerator, denominator);
    }
    
    // Check if it's a repeating decimal (contains parentheses)
    size_t paren_pos = num_str.find('(');
    if (paren_pos != std::string::npos) {
        // Parse repeating decimal: x.y(z) = x.y + 0.000...z / (10^k - 1)
        // where k is the number of repeating digits
        
        size_t close_paren = num_str.find(')', paren_pos);
        std::string repeating_part = num_str.substr(paren_pos + 1, close_paren - paren_pos - 1);
        std::string non_repeating = num_str.substr(0, paren_pos);
        
        // Parse non-repeating part
        bignum base_value;
        if (non_repeating.find('.') != std::string::npos) {
            // Convert decimal to fraction
            size_t dot_pos = non_repeating.find('.');
            std::string integer_part = non_repeating.substr(0, dot_pos);
            std::string fractional_part = non_repeating.substr(dot_pos + 1);
            
            // Handle negative numbers properly
            bool is_negative = (non_repeating[0] == '-');
            
            // Parse absolute values
            cpp_int integer_val(integer_part);
            if (is_negative) integer_val = -integer_val; // Ensure it's negative
            
            cpp_int fractional_val(fractional_part);
            cpp_int scale_factor = boost::multiprecision::pow(cpp_int(10), fractional_part.length());
            
            // Build numerator: for -x.y, we want -(x * scale + y)
            cpp_int abs_integer = boost::multiprecision::abs(integer_val);
            cpp_int numerator = abs_integer * scale_factor + fractional_val;
            if (is_negative) numerator = -numerator;
            
            base_value = bignum(numerator, scale_factor);
        } else {
            base_value = bignum(cpp_int(non_repeating));
        }
        
        // Calculate repeating part contribution
        cpp_int rep_numerator(repeating_part);
        size_t fractional_digits = 0;
        size_t dot_pos = non_repeating.find('.');
        if (dot_pos != std::string::npos) {
            fractional_digits = non_repeating.length() - dot_pos - 1;
        }
        
        cpp_int rep_denominator = boost::multiprecision::pow(cpp_int(10), fractional_digits) * 
                                  (boost::multiprecision::pow(cpp_int(10), repeating_part.length()) - 1);

        bignum repeating_value(rep_numerator, rep_denominator);
        
        // Handle negative numbers for repeating part
        if (non_repeating[0] == '-' and base_value == 0) {
            repeating_value = -repeating_value;
        }
        
        return base_value + repeating_value;
    }
    
    // Check if it's a regular decimal
    if (num_str.find('.') != std::string::npos) {
        size_t dot_pos = num_str.find('.');
        std::string integer_part = num_str.substr(0, dot_pos);
        std::string fractional_part = num_str.substr(dot_pos + 1);
        
        // Handle negative numbers properly
        bool is_negative = (num_str[0] == '-');
        
        // Parse absolute values
        cpp_int integer_val(integer_part);
        if (is_negative) integer_val = -integer_val; // Ensure it's negative
        
        cpp_int fractional_val(fractional_part);
        cpp_int scale_factor = boost::multiprecision::pow(cpp_int(10), fractional_part.length());
        
        // Build numerator: for -x.y, we want -(x * scale + y)
        cpp_int abs_integer = boost::multiprecision::abs(integer_val);
        cpp_int numerator = abs_integer * scale_factor + fractional_val;
        if (is_negative) numerator = -numerator;

        return bignum(numerator, scale_factor);
    }
    
    // Simple integer
    return bignum(cpp_int(num_str));
}

value_ptr parser::parse_expression()
{
    NOEVAL_DEBUG(parse, "Parsing token: {}", current_token.to_string());
    switch (current_token.type) {
        case token_type::left_paren:
            NOEVAL_DEBUG(parse, "Starting list parse");
            return parse_list();
            
        case token_type::symbol:
            {
                NOEVAL_DEBUG(parse, "Parsing symbol: {}", current_token.value);
                auto result = std::make_shared<value>(symbol{current_token.value});
                advance();
                return result;
            }
            
        case token_type::number:
            {
                NOEVAL_DEBUG(parse, "Parsing number: {}", current_token.value);
                bignum val = parse_number_string(current_token.value);
                auto result = std::make_shared<value>(val);
                advance();
                return result;
            }
            
        case token_type::string_literal:
            {
                NOEVAL_DEBUG(parse, "Parsing string literal: {}", current_token.value);
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

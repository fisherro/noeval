#pragma once

#include <cstddef>
#include <deque>
#include <istream>
#include <string>
#include <string_view>
#include <utility>

// A source of characters for the lexer.
//
// Characters are pulled from the underlying source only as they are needed,
// so a lexer can work over an interactive stream without reading everything
// up-front. Lookahead and pushback are both unbounded.
class char_source {
public:
    static constexpr int eof = std::char_traits<char>::eof();

    virtual ~char_source() = default;

    // Return the character `ahead` positions past the current one without
    // consuming anything, or eof if the source ends before then.
    int peek(size_t ahead = 0);

    // Consume and return the current character, or eof.
    int get();

    // Push characters back so that they are the next ones returned.
    void unget(char ch);
    void unget(std::string_view chars);

protected:
    // Read the next character from the underlying source, or eof.
    virtual int fetch() = 0;

private:
    std::deque<char> buffer_;
};

class string_source: public char_source {
public:
    explicit string_source(std::string text): text_(std::move(text)) {}

protected:
    int fetch() override;

private:
    std::string text_;
    size_t index_{0};
};

class istream_source: public char_source {
public:
    // The stream must outlive the source.
    explicit istream_source(std::istream& in): in_(in) {}

protected:
    int fetch() override;

private:
    std::istream& in_;
};

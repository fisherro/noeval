#include "char_source.hpp"

int char_source::peek(size_t ahead)
{
    while (buffer_.size() <= ahead) {
        int ch = fetch();
        if (eof == ch) return eof;
        buffer_.push_back(static_cast<char>(ch));
    }
    return static_cast<unsigned char>(buffer_[ahead]);
}

int char_source::get()
{
    int ch = peek();
    if (eof != ch) buffer_.pop_front();
    return ch;
}

void char_source::unget(char ch)
{
    buffer_.push_front(ch);
}

void char_source::unget(std::string_view chars)
{
    buffer_.insert(buffer_.begin(), chars.begin(), chars.end());
}

int string_source::fetch()
{
    if (index_ >= text_.size()) return eof;
    return static_cast<unsigned char>(text_[index_++]);
}

int istream_source::fetch()
{
    return in_.get();
}

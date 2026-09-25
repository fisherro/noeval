#pragma once

#include <cstddef>
#include <streambuf>
#include <string>

// A stream buffer that wraps another stream buffer and adds unlimited
// lookahead and pushback.
//
// Characters are read from the source only as they are needed. When the
// source can supply more characters without blocking (in_avail() > 0), they
// are read in chunks so that the sgetc()/sbumpc() fast path usually applies.
// Otherwise (a terminal, or std::cin synced with stdio) they are read one at a
// time, so reading never waits for input past what was asked for.
//
// sputbackc() accepts any character, however many have been pushed back.
class pushback_streambuf: public std::streambuf {
public:
    // The source must outlive this buffer.
    explicit pushback_streambuf(std::streambuf* source): source_(source) {}

    pushback_streambuf(const pushback_streambuf&) = delete;
    pushback_streambuf& operator=(const pushback_streambuf&) = delete;

    std::streambuf* source() const { return source_; }

    // Return the character `ahead` positions past the current one without
    // consuming anything, or eof if the source ends before then.
    int_type peek(std::size_t ahead)
    {
        if (static_cast<std::size_t>(egptr() - gptr()) > ahead or fill(ahead + 1)) {
            return traits_type::to_int_type(gptr()[ahead]);
        }
        return traits_type::eof();
    }

    // Give characters that were read ahead but not consumed back to the source,
    // as many as it will take, so that other readers of the source see them.
    void return_unread();

protected:
    int_type underflow() override;
    int_type pbackfail(int_type ch) override;
    std::streamsize showmanyc() override;

private:
    // Largest number of characters to read from the source at once
    static constexpr std::streamsize chunk_size = 4096;

    std::streambuf* source_;
    // Holds the get area. Characters before gptr() have been consumed and
    // can be overwritten by pushback. The size is always egptr() - eback().
    std::string buffer_;

    // Make at least `count` characters available at gptr(). Returns false if
    // the source ends first.
    bool fill(std::size_t count);
};

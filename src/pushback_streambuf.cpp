#include <algorithm>

#include "pushback_streambuf.hpp"

bool pushback_streambuf::fill(std::size_t count)
{
    // Discard consumed characters, keeping the unread ones.
    buffer_.erase(0, gptr() - eback());

    while (buffer_.size() < count) {
        // Wait for one character, then take whatever else is ready.
        int_type ch = source_->sbumpc();
        if (traits_type::eq_int_type(ch, traits_type::eof())) break;
        buffer_ += traits_type::to_char_type(ch);

        std::streamsize ready = std::min(source_->in_avail(), chunk_size);
        if (ready > 0) {
            buffer_.resize_and_overwrite(buffer_.size() + ready,
                [this, ready](char* data, std::size_t size) {
                    return size - ready + source_->sgetn(data + size - ready, ready);
                });
        }
    }

    char* data = buffer_.data();
    setg(data, data, data + buffer_.size());
    return buffer_.size() >= count;
}

pushback_streambuf::int_type pushback_streambuf::underflow()
{
    if (fill(1)) return traits_type::to_int_type(*gptr());
    return traits_type::eof();
}

pushback_streambuf::int_type pushback_streambuf::pbackfail(int_type ch)
{
    // Only called with eof when there is nothing before gptr() to back up to
    if (traits_type::eq_int_type(ch, traits_type::eof())) return traits_type::eof();

    if (gptr() == eback()) {
        // Make room in front of the unread characters, growing geometrically
        // so that pushing back many characters one at a time stays cheap.
        std::size_t room = std::max<std::size_t>(64, buffer_.size());
        buffer_.insert(0, room, '\0');
        char* data = buffer_.data();
        setg(data, data + room, data + buffer_.size());
    }

    gbump(-1);
    *gptr() = traits_type::to_char_type(ch);
    return ch;
}

std::streamsize pushback_streambuf::showmanyc()
{
    return source_->in_avail();
}

void pushback_streambuf::return_unread()
{
    // Put them back last first, stopping at the first one the source refuses,
    // so that the source ends up with a tail of what we hold.
    while (gptr() < egptr()) {
        if (traits_type::eq_int_type(source_->sputbackc(egptr()[-1]), traits_type::eof())) break;
        setg(eback(), gptr(), egptr() - 1);
    }
    buffer_.resize(egptr() - eback());
}

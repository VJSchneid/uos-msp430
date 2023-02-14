#pragma once


template<unsigned NrBufs>
struct buffer_sequence : private buffer_sequence<NrBufs-1> {
    using next_buffer = buffer_sequence<NrBufs-1>;

    constexpr buffer_sequence(buffer_sequence<NrBufs> const &) = default;

    constexpr buffer_sequence(buffer_sequence<NrBufs-1> const &prev, unsigned char *data, unsigned length) : next_buffer(prev) {
        data_ = data;
        length_ = length;
    }

    template<typename Func>
    constexpr bool apply(Func &&f) {
        if (next_buffer::apply(f)) {
            return f(data_, length_);
        }
        return false;
    }

    constexpr buffer_sequence<NrBufs+1> add_buffer(unsigned char *data, unsigned length) {
        return buffer_sequence<NrBufs+1>(*this, data, length);
    }

    unsigned char *data_;
    unsigned length_;
};

template<>
struct buffer_sequence<0> {
    template<typename Func>
    constexpr bool apply(Func &&) { return true; }

    constexpr buffer_sequence<1> add_buffer(unsigned char *data, unsigned length) {
        return buffer_sequence<1>{*this, data, length};
    }
};

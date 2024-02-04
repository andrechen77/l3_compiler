#pragma once

#include <charconv>

namespace utils {
	template<typename T>
    T string_view_to_int(std::string_view view) {
        T result;
        auto start = view.data();
        auto end = view.data() + view.size();
        if (view.front() == '+') {
            start += 1;
        }
        auto [ptr, ec] = std::from_chars(start, end, result);
        // TODO check for error
        return result;
    }
}

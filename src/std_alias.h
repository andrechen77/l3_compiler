#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <optional>

namespace std_alias {
	template<typename T>
	using Uptr = std::unique_ptr<T>;

	template<typename T>
	using Vec = std::vector<T>;

	template<typename T>
	decltype(auto) mv(T &&arg) { return std::move(arg); }

	template<typename T>
	using Opt = std::optional<T>;
}

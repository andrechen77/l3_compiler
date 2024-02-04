#pragma once

#include <memory>
#include <vector>
#include <utility>
#include <optional>
#include <set>
#include <map>

namespace std_alias {
	template<typename T>
	using Uptr = std::unique_ptr<T>;

	template<typename T>
	using Vec = std::vector<T>;

	template<typename T>
    using Set = std::set<T, std::less<void>>;

	template<typename K, typename V>
	using Map = std::map<K, V, std::less<void>>;

	template<typename T>
	decltype(auto) mv(T &&arg) { return std::move(arg); }

	template<typename T>
	using Opt = std::optional<T>;

	template<typename T1, typename T2>
	using Pair = std::pair<T1, T2>;
}

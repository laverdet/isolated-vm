module;
#include <cstdint>
export module ivm.napi:number;
import :value;
import napi;

namespace js::napi {

// number constructors
template <>
struct factory<number_tag> : factory<value_tag> {
		using factory<value_tag>::factory;
		auto operator()(double number) const -> value<number_tag_of<double>>;
		auto operator()(int32_t number) const -> value<number_tag_of<int32_t>>;
		auto operator()(int64_t number) const -> value<number_tag_of<int64_t>>;
		auto operator()(uint32_t number) const -> value<number_tag_of<uint32_t>>;
};

template <>
struct factory<number_tag_of<double>> : factory<number_tag> {
		using factory<number_tag>::factory;
};

template <>
struct factory<number_tag_of<int32_t>> : factory<number_tag> {
		using factory<number_tag>::factory;
};

template <>
struct factory<number_tag_of<int64_t>> : factory<number_tag> {
		using factory<number_tag>::factory;
};

template <>
struct factory<number_tag_of<uint32_t>> : factory<number_tag> {
		using factory<number_tag>::factory;
};

// bigint constructors
template <>
struct factory<bigint_tag> : factory<value_tag> {
	public:
		using factory<value_tag>::factory;
		auto operator()(int64_t number) const -> value<bigint_tag_of<int64_t>>;
		auto operator()(uint64_t number) const -> value<bigint_tag_of<uint64_t>>;
};

template <>
struct factory<bigint_tag_of<int64_t>> : factory<bigint_tag> {
		using factory<bigint_tag>::factory;
};

template <>
struct factory<bigint_tag_of<uint64_t>> : factory<bigint_tag> {
		using factory<bigint_tag>::factory;
};

} // namespace js::napi

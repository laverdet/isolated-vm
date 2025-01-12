module;
#include <cstdint>
#include <type_traits>
export module ivm.iv8:number;
import v8;

namespace js::iv8 {

export class number : public v8::Number {
	public:
		[[nodiscard]] auto materialize(std::type_identity<double> tag) const -> double;
		[[nodiscard]] auto materialize(std::type_identity<int32_t> tag) const -> int32_t;
		[[nodiscard]] auto materialize(std::type_identity<int64_t> tag) const -> int64_t;
		[[nodiscard]] auto materialize(std::type_identity<uint32_t> tag) const -> uint32_t;
		static auto Cast(v8::Data* data) -> number*;

	private:
		template <class Type>
		auto cast() const -> const Type*;
};

auto number::Cast(v8::Data* data) -> number* {
	return reinterpret_cast<number*>(v8::Number::Cast(data));
}

auto number::materialize(std::type_identity<double> /*tag*/) const -> double {
	return Value();
}

auto number::materialize(std::type_identity<int32_t> /*tag*/) const -> int32_t {
	return cast<v8::Int32>()->Value();
}

auto number::materialize(std::type_identity<int64_t> /*tag*/) const -> int64_t {
	return cast<v8::Integer>()->Value();
}

auto number::materialize(std::type_identity<uint32_t> /*tag*/) const -> uint32_t {
	return cast<v8::Uint32>()->Value();
}

template <class Type>
auto number::cast() const -> const Type* {
	return Type::Cast(const_cast<number*>(this)); // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

} // namespace js::iv8

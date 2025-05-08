module;
#include <cstdint>
#include <type_traits>
export module v8_js.number;
import isolated_js;
import v8_js.handle;
import v8;

namespace js::iv8 {

export class number
		: public v8::Local<v8::Number>,
			public materializable<number> {
	public:
		explicit number(v8::Local<v8::Number> handle) :
				Local{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<double> tag) const -> double;
		[[nodiscard]] auto materialize(std::type_identity<int32_t> tag) const -> int32_t;
		[[nodiscard]] auto materialize(std::type_identity<int64_t> tag) const -> int64_t;
		[[nodiscard]] auto materialize(std::type_identity<uint32_t> tag) const -> uint32_t;
};

// ---

auto number::materialize(std::type_identity<double> /*tag*/) const -> double {
	return (*this)->Value();
}

auto number::materialize(std::type_identity<int32_t> /*tag*/) const -> int32_t {
	return this->As<v8::Int32>()->Value();
}

auto number::materialize(std::type_identity<int64_t> /*tag*/) const -> int64_t {
	return this->As<v8::Integer>()->Value();
}

auto number::materialize(std::type_identity<uint32_t> /*tag*/) const -> uint32_t {
	return this->As<v8::Uint32>()->Value();
}

} // namespace js::iv8

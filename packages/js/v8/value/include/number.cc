module;
#include <cstdint>
#include <type_traits>
export module v8_js:number;
import :handle;
import isolated_js;
import v8;

namespace js::iv8 {

export class number
		: public v8::Local<v8::Number>,
			public materializable<number> {
	public:
		explicit number(v8::Local<v8::Number> handle) :
				v8::Local<v8::Number>{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<double> tag) const -> double;
		[[nodiscard]] auto materialize(std::type_identity<int32_t> tag) const -> int32_t;
		[[nodiscard]] auto materialize(std::type_identity<int64_t> tag) const -> int64_t;
		[[nodiscard]] auto materialize(std::type_identity<uint32_t> tag) const -> uint32_t;
};

export class bigint_n
		: public v8::Local<v8::BigInt>,
			public materializable<bigint_n> {
	public:
		explicit bigint_n(v8::Local<v8::BigInt> handle) :
				v8::Local<v8::BigInt>{handle} {}

		[[nodiscard]] auto materialize(std::type_identity<bigint> tag) const -> bigint;
};

export class bigint_u64
		: public v8::Local<v8::BigInt>,
			public materializable<bigint_u64> {
	public:
		explicit bigint_u64(v8::Local<v8::BigInt> handle, uint64_t value) :
				v8::Local<v8::BigInt>{handle},
				value_{value} {}

		[[nodiscard]] auto materialize(std::type_identity<uint64_t> tag) const -> uint64_t;

	private:
		uint64_t value_;
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

auto bigint_n::materialize(std::type_identity<bigint> /*tag*/) const -> bigint {
	auto bigint = js::bigint{};
	bigint.resize_and_overwrite((*this)->WordCount(), [ & ](auto* words, auto length) {
		if (length > 0) {
			auto int_length = static_cast<int>(length);
			(*this)->ToWordsArray(&bigint.sign_bit(), &int_length, words);
		}
		return length;
	});
	return bigint;
}

auto bigint_u64::materialize(std::type_identity<uint64_t> /*tag*/) const -> uint64_t {
	return value_;
}

} // namespace js::iv8

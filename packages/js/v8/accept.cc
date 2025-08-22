module;
#ifdef _LIBCPP_VERSION
// nb: Symbol visibility hack
#include <__iterator/wrap_iter.h>
#endif
#include <concepts>
#include <cstdint>
#include <string_view>
#include <utility>
export module v8_js.accept;
import isolated_js;
import ivm.utility;
import v8_js.date;
import v8_js.lock;
import v8_js.string;
import v8;

namespace js {

// Base class for primitive acceptors. Not really an acceptor.
template <>
struct accept<void, v8::Local<v8::Data>> {
	public:
		accept() = delete;
		explicit accept(const js::iv8::isolate_lock_witness& lock) :
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto isolate() const {
			return isolate_;
		}

		// undefined & null
		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> v8::Local<v8::Value> {
			return v8::Undefined(isolate());
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const -> v8::Local<v8::Value> {
			return v8::Null(isolate());
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, auto&& value) const -> v8::Local<v8::Boolean> {
			return v8::Boolean::New(isolate_, std::forward<decltype(value)>(value));
		}

		// number
		auto operator()(number_tag /*tag*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, std::forward<decltype(value)>(value));
		}

		auto operator()(number_tag_of<int32_t> /*tag*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::New(isolate_, std::forward<decltype(value)>(value));
		}

		auto operator()(number_tag_of<uint32_t> /*tag*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::NewFromUnsigned(isolate_, std::forward<decltype(value)>(value));
		}

		// string
		auto operator()(string_tag /*tag*/, auto&& value) const -> v8::Local<v8::String> {
			return iv8::string::make(isolate_, std::u16string_view{std::forward<decltype(value)>(value)});
		}

		auto operator()(string_tag_of<char> /*tag*/, auto&& value) const -> v8::Local<v8::String> {
			return iv8::string::make(isolate_, std::string_view{std::forward<decltype(value)>(value)});
		}

	private:
		v8::Isolate* isolate_;
};

// Explicit boolean acceptor
template <>
struct accept<void, v8::Local<v8::Boolean>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;
		auto operator()(boolean_tag tag, auto&& value) const -> v8::Local<v8::Boolean> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}
};

// Explicit numeric acceptor
template <>
struct accept<void, v8::Local<v8::Number>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;

		auto operator()(number_tag tag, auto&& value) const -> v8::Local<v8::Number> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> tag, auto&& value) const -> v8::Local<v8::Number> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}
};

// Explicit string acceptor
template <>
struct accept<void, v8::Local<v8::String>> : accept<void, v8::Local<v8::Data>> {
		using accept<void, v8::Local<v8::Data>>::accept;

		auto operator()(string_tag tag, auto&& value) const -> v8::Local<v8::String> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, auto&& value) const -> v8::Local<v8::String> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}
};

// Generic acceptor for most values
template <>
struct accept<void, v8::Local<v8::Value>> : accept<void, v8::Local<v8::Data>> {
	public:
		accept() = delete;
		explicit accept(const iv8::context_lock_witness& lock) :
				accept<void, v8::Local<v8::Data>>{lock},
				context_{lock.context()} {}

		[[nodiscard]] auto context() const {
			return context_;
		}

		auto operator()(auto_tag auto tag, auto&& value) const -> v8::Local<v8::Value>
			requires std::invocable<accept<void, v8::Local<v8::Data>>, decltype(tag), decltype(value)> {
			return accept<void, v8::Local<v8::Data>>::operator()(tag, std::forward<decltype(value)>(value));
		}

		// hacky function template acceptor
		auto operator()(function_tag /*tag*/, v8::Local<v8::Value> value) const -> v8::Local<v8::Value> {
			return value;
		}

		// array
		auto operator()(list_tag /*tag*/, auto&& list, const auto& visit) const -> v8::Local<v8::Value> {
			auto array = v8::Array::New(isolate());
			for (auto&& [ key, value ] : list) {
				auto result = array->Set(
					context_,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return array;
		}

		// object
		auto operator()(date_tag /*tag*/, auto&& value) const -> v8::Local<v8::Value> {
			return iv8::date::make(context_, std::forward<decltype(value)>(value));
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary, const auto& visit) const -> v8::Local<v8::Value> {
			auto object = v8::Object::New(isolate());
			for (auto&& [ key, value ] : dictionary) {
				auto result = object->Set(
					context_,
					visit.first(std::forward<decltype(key)>(key), *this),
					visit.second(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return object;
		}

	private:
		v8::Local<v8::Context> context_;
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Meta, class Type>
struct accept<Meta, v8::MaybeLocal<Type>> : accept<Meta, v8::Local<Type>> {
		using accept<Meta, v8::Local<Type>>::accept;
		using accept_type = accept<Meta, v8::Local<Type>>;

		auto operator()(auto_tag auto tag, auto&& value, auto&&... rest) const -> v8::MaybeLocal<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value), decltype(rest)...> {
			const accept_type& accept = *this;
			return accept(tag, std::forward<decltype(value)>(value), std::forward<decltype(rest)>(rest)...);
		}

		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> v8::MaybeLocal<Type> {
			return v8::MaybeLocal<Type>{};
		}
};

// return value (actually a c++ void return)
template <>
struct accept<void, v8::ReturnValue<v8::Value>> : accept<void, v8::Local<v8::Value>> {
	public:
		using accept_type = accept<void, v8::Local<v8::Value>>;

		accept(const iv8::context_lock_witness& lock, v8::ReturnValue<v8::Value> return_value) :
				accept<void, v8::Local<v8::Value>>{lock},
				return_value_{return_value} {}

		auto operator()(boolean_tag /*tag*/, const auto& value) const -> void {
			return_value_.Set(static_cast<bool>(value));
		}

		template <class Type>
		auto operator()(number_tag_of<Type> /*tag*/, const auto& value) const -> void {
			return_value_.Set(static_cast<Type>(value));
		}

		auto operator()(null_tag /*tag*/, const auto& /*null*/) const -> void {
			return_value_.SetNull();
		}

		auto operator()(undefined_tag /*tag*/, const auto& /*undefined*/) const -> void {
			return_value_.SetUndefined();
		}

		auto operator()(auto_tag auto tag, auto&& value) const -> void
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			return_value_.Set(accept_type::operator()(tag, std::forward<decltype(value)>(value)));
		}

	private:
		mutable v8::ReturnValue<v8::Value> return_value_;
};

} // namespace js

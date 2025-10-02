module;
#ifdef _LIBCPP_VERSION
// nb: Symbol visibility hack
#include <__iterator/wrap_iter.h>
#endif
#include <concepts>
#include <cstdint>
#include <string_view>
#include <utility>
export module v8_js:accept;
import :date;
import :lock;
import :string;
import isolated_js;
import ivm.utility;
import v8;

namespace js {

// Base class for primitive acceptors. These require only an isolate lock.
struct accept_v8_primitive {
	public:
		accept_v8_primitive() = delete;
		explicit accept_v8_primitive(const js::iv8::isolate_lock_witness& lock) :
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto isolate() const {
			return isolate_;
		}

		// undefined & null
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> v8::Local<v8::Primitive> {
			return v8::Undefined(isolate());
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> v8::Local<v8::Primitive> {
			return v8::Null(isolate());
		}

		// boolean
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::Boolean> {
			return v8::Boolean::New(isolate_, std::forward<decltype(value)>(value));
		}

		// number
		auto operator()(number_tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, std::forward<decltype(value)>(value));
		}

		auto operator()(number_tag_of<int32_t> /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::New(isolate_, std::forward<decltype(value)>(value));
		}

		auto operator()(number_tag_of<uint32_t> /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::NewFromUnsigned(isolate_, std::forward<decltype(value)>(value));
		}

		// string
		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::String> {
			return iv8::string::make(isolate_, std::basic_string_view<Char>{std::forward<decltype(value)>(value)});
		}

	private:
		v8::Isolate* isolate_;
};

// Explicit boolean acceptor
template <>
struct accept<void, v8::Local<v8::Boolean>> : accept_v8_primitive {
		using accept_v8_primitive::accept_v8_primitive;
		auto operator()(boolean_tag tag, const auto& visit, auto&& value) const -> v8::Local<v8::Boolean> {
			return accept_v8_primitive::operator()(tag, visit, std::forward<decltype(value)>(value));
		}
};

// Explicit numeric acceptor
template <>
struct accept<void, v8::Local<v8::Number>> : accept_v8_primitive {
		using accept_v8_primitive::accept_v8_primitive;
		auto operator()(auto_tag<number_tag> auto tag, const auto& visit, auto&& value) const -> v8::Local<v8::Number> {
			return accept_v8_primitive::operator()(tag, visit, std::forward<decltype(value)>(value));
		}
};

// Explicit string acceptor
template <>
struct accept<void, v8::Local<v8::String>> : accept_v8_primitive {
		using accept_v8_primitive::accept_v8_primitive;
		auto operator()(auto_tag<string_tag> auto tag, const auto& visit, auto&& value) const -> v8::Local<v8::String> {
			return accept_v8_primitive::operator()(tag, visit, std::forward<decltype(value)>(value));
		}
};

// Generic acceptor for most values
template <>
struct accept<void, v8::Local<v8::Value>> : accept_v8_primitive {
	public:
		accept() = delete;
		explicit accept(const iv8::context_lock_witness& lock) :
				accept_v8_primitive{lock},
				context_{lock.context()} {}

		[[nodiscard]] auto context() const {
			return context_;
		}

		// accept all primitives
		using accept_v8_primitive::operator();

		// hacky function template acceptor
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> value) const -> v8::Local<v8::Function> {
			return value;
		}

		// array
		auto operator()(list_tag /*tag*/, const auto& visit, auto&& list) const -> v8::Local<v8::Array> {
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
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, auto&& value) const -> v8::Local<v8::Date> {
			return iv8::date::make(context_, std::forward<decltype(value)>(value));
		}

		auto operator()(dictionary_tag /*tag*/, const auto& visit, auto&& dictionary) const -> v8::Local<v8::Object> {
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
		using accept_type = accept<Meta, v8::Local<Type>>;
		using accept_type::accept_type;

		using accept_type::operator();
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> v8::MaybeLocal<Type> {
			return {};
		}
};

// return value
template <>
struct accept<void, v8::ReturnValue<v8::Value>> : accept<void, v8::Local<v8::Value>> {
		using value_type = v8::ReturnValue<v8::Value>;
		using accept_type = accept<void, v8::Local<v8::Value>>;
		accept(const iv8::context_lock_witness& lock, value_type return_value) :
				accept_type{lock},
				return_value_{return_value} {}

		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, const auto& value) const -> value_type {
			return_value_.Set(bool{value});
			return return_value_;
		}

		template <class Type>
		auto operator()(number_tag_of<Type> /*tag*/, visit_holder /*visit*/, const auto& value) const -> value_type {
			return_value_.Set(Type{value});
			return return_value_;
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) const -> value_type {
			return_value_.SetNull();
			return return_value_;
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> value_type {
			return_value_.SetUndefined();
			return return_value_;
		}

		auto operator()(auto_tag auto tag, const auto& visit, auto&& value) const -> value_type
			requires std::invocable<const accept_type&, decltype(visit), decltype(tag), decltype(value)> {
			return_value_.Set(accept_type::operator()(tag, visit, std::forward<decltype(value)>(value)));
			return return_value_;
		}

	private:
		mutable v8::ReturnValue<v8::Value> return_value_;
};

} // namespace js

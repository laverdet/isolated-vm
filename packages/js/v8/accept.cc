module;
#ifdef _LIBCPP_VERSION
// nb: Symbol visibility hack
#include <__iterator/wrap_iter.h>
#endif
#include <concepts>
#include <cstdint>
#include <string_view>
#include <tuple>
#include <utility>
export module v8_js:accept;
import :date;
import :hash;
import :lock;
import :string;
import isolated_js;
import ivm.utility;
import v8;

namespace js {

// Base class for primitive acceptors. These require only an isolate lock.
struct accept_v8_primitive {
	public:
		using accept_reference_type = v8::Local<v8::Value>;

		accept_v8_primitive() = delete;
		explicit accept_v8_primitive(const js::iv8::isolate_lock_witness& lock) :
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

		// reference provider
		template <class Type>
		static auto reaccept(std::type_identity<v8::Local<Type>> /*type*/, v8::Local<v8::Value> value) -> v8::Local<Type> {
			return value.As<Type>();
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
		auto operator()(number_tag /*tag*/, visit_holder /*visit*/, std::convertible_to<double> auto&& value) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, double{std::forward<decltype(value)>(value)});
		}

		auto operator()(number_tag_of<int32_t> /*tag*/, visit_holder /*visit*/, std::convertible_to<int32_t> auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::New(isolate_, int32_t{std::forward<decltype(value)>(value)});
		}

		auto operator()(number_tag_of<uint32_t> /*tag*/, visit_holder /*visit*/, std::convertible_to<uint32_t> auto&& value) const -> v8::Local<v8::Number> {
			return v8::Int32::NewFromUnsigned(isolate_, uint32_t{std::forward<decltype(value)>(value)});
		}

		// string
		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, std::convertible_to<std::basic_string_view<Char>> auto&& value) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{iv8::string::make(isolate_, std::basic_string_view<Char>{std::forward<decltype(value)>(value)})};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& value) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{iv8::string::make(isolate_, std::basic_string<Char>{std::forward<decltype(value)>(value)})};
		}

	private:
		v8::Isolate* isolate_;
};

// Explicit string acceptor
template <>
struct accept<void, v8::Local<v8::String>> : accept_v8_primitive {
		explicit accept(const js::iv8::isolate_lock_witness& lock) :
				accept_v8_primitive{lock} {}
};

// Generic acceptor for most values. These require a context lock.
struct accept_v8_value : accept_v8_primitive {
	public:
		using accept_type = accept_v8_primitive;
		explicit accept_v8_value(const iv8::context_lock_witness& lock) :
				accept_type{lock},
				context_{lock.context()} {}

		// accept all primitives
		using accept_type::operator();

		// hacky function template acceptor
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> value) const -> v8::Local<v8::Function> {
			return value;
		}

		// date
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, std::convertible_to<js_clock::time_point> auto&& value) const
			-> js::referenceable_value<v8::Local<v8::Date>> {
			return js::referenceable_value<v8::Local<v8::Date>>{
				iv8::date::make(context_, js_clock::time_point{std::forward<decltype(value)>(value)}),
			};
		}

		// array
		auto operator()(this auto& self, list_tag /*tag*/, const auto& visit, auto&& list)
			-> js::deferred_receiver<v8::Local<v8::Array>, decltype(self), decltype(visit), decltype(list)> {
			return {
				v8::Array::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(list)>(list)),
				[](v8::Local<v8::Array> array, auto& self, const auto& visit, auto /*&&*/ list) -> void {
					for (auto&& [ key, value ] : util::into_range(std::forward<decltype(list)>(list))) {
						auto result = array->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						result.Check();
					}
				},
			};
		}

		// object
		auto operator()(this auto& self, dictionary_tag /*tag*/, const auto& visit, auto&& dictionary)
			-> js::deferred_receiver<v8::Local<v8::Object>, decltype(self), decltype(visit), decltype(dictionary)> {
			return {
				v8::Object::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(dictionary)>(dictionary)),
				[](v8::Local<v8::Object> object, auto& self, const auto& visit, auto /*&&*/ dictionary) -> void {
					for (auto&& [ key, value ] : util::into_range(std::forward<decltype(dictionary)>(dictionary))) {
						auto result = object->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						result.Check();
					}
				},
			};
		}

	private:
		v8::Local<v8::Context> context_;
};

template <>
struct accept<void, v8::Local<v8::Value>> : accept_v8_value {
		explicit constexpr accept(const iv8::context_lock_witness& lock) :
				accept_v8_value{lock} {}
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Type>
struct accept<void, v8::MaybeLocal<Type>> : accept<void, v8::Local<Type>> {
		using accept_type = accept<void, v8::Local<Type>>;
		using accept_type::accept_type;

		using accept_type::operator();
		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) const -> v8::MaybeLocal<Type> {
			return {};
		}
};

// return value
template <>
struct accept<void, v8::ReturnValue<v8::Value>> : accept<void, v8::Local<v8::Value>> {
		using accept_type = accept<void, v8::Local<v8::Value>>;
		using accept_type::accept_type;
		using value_type = v8::ReturnValue<v8::Value>;

		accept(const iv8::context_lock_witness& lock, value_type return_value) :
				accept_type{lock},
				return_value_{return_value} {}

		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, const auto& value) -> value_type {
			return_value_.Set(bool{value});
			return return_value_;
		}

		template <class Type>
		auto operator()(number_tag_of<Type> /*tag*/, visit_holder /*visit*/, const auto& value) -> value_type {
			return_value_.Set(Type{value});
			return return_value_;
		}

		auto operator()(null_tag /*tag*/, visit_holder /*visit*/, const auto& /*null*/) -> value_type {
			return_value_.SetNull();
			return return_value_;
		}

		auto operator()(undefined_tag /*tag*/, visit_holder /*visit*/, const auto& /*undefined*/) -> value_type {
			return_value_.SetUndefined();
			return return_value_;
		}

		auto operator()(auto_tag auto tag, const auto& visit, auto&& value) -> value_type
			requires std::invocable<accept_type&, decltype(visit), decltype(tag), decltype(value)> {
			return_value_.Set(util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(value)>(value)));
			return return_value_;
		}

	private:
		v8::ReturnValue<v8::Value> return_value_;
};

} // namespace js

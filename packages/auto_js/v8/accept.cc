module;
#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
export module v8_js:accept;
import :hash;
import :lock;
import :primitive;
import :unmaybe;
import auto_js;
import util;
import v8;

namespace js {

// Base class for primitive acceptors. These require only an isolate lock.
struct accept_v8_primitive {
	public:
		// nb: This marks this acceptor as referential!
		using accept_reference_type = v8::Local<v8::Value>;

		accept_v8_primitive() = delete;
		explicit accept_v8_primitive(iv8::isolate_lock_witness lock) :
				isolate_{lock.isolate()} {}
		explicit accept_v8_primitive(const std::convertible_to<iv8::isolate_lock_witness> auto& lock) :
				accept_v8_primitive{iv8::isolate_lock_witness{util::slice(lock)}} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }

		// reference provider
		template <class Type>
		constexpr auto operator()(std::type_identity<v8::Local<Type>> /*type*/, v8::Local<v8::Value> value) const -> v8::Local<Type> {
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
		auto operator()(boolean_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Boolean> {
			return v8::Boolean::New(isolate_, std::forward<decltype(subject)>(subject));
		}

		// number
		auto operator()(number_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Number> {
			return v8::Number::New(isolate_, double{std::forward<decltype(subject)>(subject)});
		}

		auto operator()(number_tag_of<int32_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Number> {
			return v8::Int32::New(isolate_, int32_t{std::forward<decltype(subject)>(subject)});
		}

		auto operator()(number_tag_of<uint32_t> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::Number> {
			return v8::Int32::NewFromUnsigned(isolate_, uint32_t{std::forward<decltype(subject)>(subject)});
		}

		// string
		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{iv8::string::make(isolate_, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)})};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::String>> {
			return js::referenceable_value{iv8::string::make(isolate_, std::basic_string<Char>{std::forward<decltype(subject)>(subject)})};
		}

		// function (instantiated in visitor)
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> subject) const -> v8::Local<v8::Function> {
			return subject;
		}

		// no required types
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		v8::Isolate* isolate_;
};

// Explicit string acceptor
template <>
struct accept<void, v8::Local<v8::String>> : accept_v8_primitive {
		using accept_v8_primitive::accept_v8_primitive;
};

// Generic acceptor for most values. These require a context lock.
struct accept_v8_value : accept_v8_primitive {
	public:
		using accept_type = accept_v8_primitive;
		explicit accept_v8_value(iv8::context_lock_witness lock) :
				accept_type{lock},
				context_{lock.context()} {}
		explicit accept_v8_value(const std::convertible_to<iv8::context_lock_witness> auto& lock) :
				accept_v8_value{iv8::context_lock_witness{util::slice(lock)}} {}

		// accept all primitives
		using accept_type::operator();

		[[nodiscard]] auto witness() const -> iv8::context_lock_witness {
			auto isolate_lock = iv8::isolate_lock_witness::make_witness(isolate());
			return iv8::context_lock_witness::make_witness(isolate_lock, context_);
		}

		// hacky function template acceptor
		// NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, v8::Local<v8::Function> value) const -> v8::Local<v8::Function> {
			return value;
		}

		// bigint (why does `NewFromWords` need a context?)
		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			return (*this)(bigint_tag_of<bigint>{}, visit, bigint{std::forward<decltype(subject)>(subject)});
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			return js::referenceable_value{iv8::bigint::make(witness(), js::bigint{std::forward<decltype(subject)>(subject)})};
		}

		auto operator()(bigint_tag_of<bigint> /*tag*/, visit_holder /*visit*/, const js::bigint& subject) const
			-> js::referenceable_value<v8::Local<v8::BigInt>> {
			return js::referenceable_value{iv8::bigint::make(witness(), subject)};
		}

		template <class Numeric>
		auto operator()(bigint_tag_of<Numeric> /*tag*/, visit_holder /*visit*/, auto&& subject) const -> v8::Local<v8::BigInt> {
			return iv8::bigint::make(witness(), Numeric{std::forward<decltype(subject)>(subject)});
		}

		// date
		auto operator()(date_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::Date>> {
			return js::referenceable_value<v8::Local<v8::Date>>{
				iv8::date::make(context_, js_clock::time_point{std::forward<decltype(subject)>(subject)}),
			};
		}

		// error
		auto operator()(error_tag /*tag*/, visit_holder /*visit*/, const auto& subject) const
			-> js::referenceable_value<v8::Local<v8::Object>> {
			auto message = iv8::string::make(isolate(), subject.message());
			auto error = v8::Local<v8::Value>{[ & ]() -> v8::Local<v8::Value> {
				switch (subject.name()) {
					// These functions don't need an isolate somehow?
					default:
						return v8::Exception::Error(message);
					case js::error_name::range_error:
						return v8::Exception::RangeError(message);
					case js::error_name::reference_error:
						return v8::Exception::ReferenceError(message);
					case js::error_name::syntax_error:
						return v8::Exception::SyntaxError(message);
					case js::error_name::type_error:
						return v8::Exception::TypeError(message);
				}
			}()};
			return js::referenceable_value{error.As<v8::Object>()};
		}

		// array
		auto operator()(this const auto& self, list_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Array>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Array::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Array> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						auto result = array->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						iv8::unmaybe(result);
					}
				},
			};
		}

		// object
		auto operator()(this const auto& self, dictionary_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<v8::Local<v8::Object>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				v8::Object::New(self.isolate()),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](v8::Local<v8::Object> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
						auto result = object->Set(
							self.context_,
							visit.first(std::forward<decltype(key)>(key), self),
							visit.second(std::forward<decltype(value)>(value), self)
						);
						iv8::unmaybe(result);
					}
				},
			};
		}

		// typed arrays
		auto operator()(array_buffer_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::ArrayBuffer>> {
			auto data = js::data_block{std::forward<decltype(subject)>(subject)};
			throw std::logic_error{"unimplemented"};
		}

		auto operator()(shared_array_buffer_tag /*tag*/, visit_holder /*visit*/, auto&& subject) const
			-> js::referenceable_value<v8::Local<v8::ArrayBuffer>> {
			auto data = js::data_block{std::forward<decltype(subject)>(subject)};
			throw std::logic_error{"unimplemented"};
		}

	private:
		v8::Local<v8::Context> context_;
};

template <>
struct accept<void, v8::Local<v8::Value>> : accept_v8_value {
		using accept_v8_value::accept_v8_value;
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

		accept(iv8::context_lock_witness lock, value_type return_value) :
				accept_type{lock},
				return_value_{return_value} {}
		explicit accept(const std::convertible_to<iv8::context_lock_witness> auto& lock, value_type return_value) :
				accept{iv8::context_lock_witness{util::slice(lock)}, return_value} {}

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

		auto operator()(auto_tag auto tag, auto& visit, auto&& subject) const -> value_type
			requires std::invocable<const accept_type&, decltype(visit), decltype(tag), decltype(subject)> {
			return_value_.Set(util::invoke_as<accept_type>(*this, tag, visit, std::forward<decltype(subject)>(subject)));
			return return_value_;
		}

	private:
		mutable v8::ReturnValue<v8::Value> return_value_;
};

} // namespace js

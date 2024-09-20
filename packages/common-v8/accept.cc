module;
#ifdef _LIBCPP_VERSION
// nb: Symbol visibility hack
#include <__iterator/wrap_iter.h>
#endif
#include <utility>
export module ivm.v8:accept;
import :date;
import :string;
import ivm.value;
import v8;

namespace ivm::iv8 {

// Base class for primitive acceptors.
struct accept_primitive_base {
		accept_primitive_base() = delete;
		explicit accept_primitive_base(v8::Isolate* isolate) :
				isolate{isolate} {}

		v8::Isolate* isolate;
};

} // namespace ivm::iv8

namespace ivm::value {

// Explicit boolean acceptor
template <class Meta>
struct accept<Meta, v8::Local<v8::Boolean>> : iv8::accept_primitive_base {
		using accept_primitive_base::accept_primitive_base;
		auto operator()(boolean_tag /*tag*/, auto&& value) const {
			return v8::Boolean::New(isolate, std::forward<decltype(value)>(value));
		}
};

// Explicit numeric acceptor
template <class Meta>
struct accept<Meta, v8::Local<v8::Number>> : iv8::accept_primitive_base {
		using accept_primitive_base::accept_primitive_base;
		template <class Numeric>
		auto operator()(number_tag_of<Numeric> /*tag*/, auto&& value) const {
			return v8::Number::New(isolate, Numeric{value});
		}
};

// Explicit string acceptor
template <class Meta>
struct accept<Meta, v8::Local<v8::String>> : iv8::accept_primitive_base {
		using accept_primitive_base::accept_primitive_base;
		auto operator()(string_tag /*tag*/, auto&& value) const {
			return iv8::string::make(isolate, std::forward<decltype(value)>(value));
		}
};

// Generic acceptor for most values
template <class Meta>
struct accept<Meta, v8::Local<v8::Value>> : iv8::accept_primitive_base {
		explicit accept(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				accept_primitive_base{isolate},
				context{context} {}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> v8::Local<v8::Value> {
			return v8::Undefined(isolate);
		}

		auto operator()(null_tag /*tag*/, auto&& /*null*/) const -> v8::Local<v8::Value> {
			return v8::Null(isolate);
		}

		auto operator()(boolean_tag tag, auto&& value) const -> v8::Local<v8::Value> {
			return delegate_accept<v8::Local<v8::Boolean>>(*this, tag, std::forward<decltype(value)>(value), isolate);
		}

		template <class Numeric>
		auto operator()(number_tag_of<Numeric> tag, auto&& value) const -> v8::Local<v8::Value> {
			return delegate_accept<v8::Local<v8::Number>>(*this, tag, std::forward<decltype(value)>(value), isolate);
		}

		auto operator()(string_tag tag, auto&& value) const -> v8::Local<v8::Value> {
			return delegate_accept<v8::Local<v8::String>>(*this, tag, std::forward<decltype(value)>(value), isolate);
		}

		auto operator()(date_tag /*tag*/, auto&& value) const -> v8::Local<v8::Value> {
			return iv8::date::make(context, std::forward<decltype(value)>(value));
		}

		auto operator()(list_tag /*tag*/, auto&& list) const -> v8::Local<v8::Value> {
			auto array = v8::Array::New(isolate);
			for (auto&& [ key, value ] : list) {
				auto result = array->Set(
					context,
					invoke_visit(std::forward<decltype(key)>(key), *this),
					invoke_visit(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return array;
		}

		auto operator()(dictionary_tag /*tag*/, auto&& dictionary) const -> v8::Local<v8::Value> {
			auto object = v8::Object::New(isolate);
			for (auto&& [ key, value ] : dictionary) {
				auto result = object->Set(
					context,
					invoke_visit(std::forward<decltype(key)>(key), *this),
					invoke_visit(std::forward<decltype(value)>(value), *this)
				);
				result.Check();
			}
			return object;
		}

		v8::Local<v8::Context> context;
};

// A `MaybeLocal` also accepts `undefined`, similar to `std::optional`.
template <class Meta, class Type>
struct accept<Meta, v8::MaybeLocal<Type>> : accept<Meta, v8::Local<Type>> {
		using accept<Meta, v8::Local<Type>>::accept;
		using accept_type = accept<Meta, v8::Local<Type>>;

		constexpr auto operator()(auto_tag auto tag, auto&& value) const -> v8::MaybeLocal<Type>
			requires std::invocable<accept_type, decltype(tag), decltype(value)> {
			return {accept_type::operator()(tag, std::forward<decltype(value)>(value))};
		}

		auto operator()(undefined_tag /*tag*/, auto&& /*undefined*/) const -> v8::MaybeLocal<Type> {
			return v8::MaybeLocal<Type>{};
		}
};

}; // namespace ivm::value

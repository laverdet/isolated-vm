module;
#include <type_traits>
#include <utility>
#include <variant>
export module ivm.napi:visit;
import :arguments;
import :object;
import :string;
import ivm.iv8;
import ivm.utility;
import ivm.value;
import napi;
import v8;

namespace ivm::value {

// Delegate napi_value to various visitors
template <>
struct visit<void, napi_value> : visit<void, v8::Local<v8::Value>> {
	public:
		using visit<void, v8::Local<v8::Value>>::operator();

		visit(napi_env env, v8::Isolate* isolate, v8::Local<v8::Context> context) :
				visit<void, v8::Local<v8::Value>>{isolate, context},
				env_{env} {}
		visit(napi_env env, v8::Isolate* isolate) :
				visit{env, isolate, isolate->GetCurrentContext()} {}

		auto env() const -> napi_env {
			return env_;
		}

		auto operator()(napi_value value, const auto_accept auto& accept) const -> decltype(auto) {
			switch (napi_invoke_checked(napi_typeof, env_, value)) {
				case napi_boolean:
					return (*this)(napi_to_v8(value).As<v8::Boolean>(), accept);
				case napi_number:
					return (*this)(napi_to_v8(value).As<v8::Number>(), accept);
				case napi_bigint:
					return (*this)(napi_to_v8(value).As<v8::BigInt>(), accept);
				case napi_string:
					return (*this)(napi_to_v8(value).As<v8::String>(), accept);
				case napi_object:
					{
						auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
						if (napi_invoke_checked(napi_is_array, env_, value)) {
							return accept(list_tag{}, ivm::napi::object{env_, value}, visit_entry);
						} else if (napi_invoke_checked(napi_is_date, env_, value)) {
							return (*this)(napi_to_v8(value).As<v8::Date>(), accept);
						} else if (napi_invoke_checked(napi_is_promise, env_, value)) {
							return accept(promise_tag{}, value);
						}
						return accept(dictionary_tag{}, ivm::napi::object{env_, value}, visit_entry);
					}
				case napi_external:
					return (*this)(napi_to_v8(value).As<v8::External>(), accept);
				case napi_symbol:
					return accept(symbol_tag{}, value);
				case napi_null:
					return accept(null_tag{}, std::nullptr_t{});
				case napi_undefined:
					return accept(undefined_tag{}, std::monostate{});
				case napi_function:
					return accept(value_tag{}, std::type_identity<void>{});
			}
		}

		auto operator()(napi_callback_info info, const auto_accept auto& accept) const -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{env_, info}, *this);
		}

	private:
		napi_env env_;
};

// Object key maker via napi
template <util::string_literal Key>
struct visit<void, key_for<Key, napi_value>> {
	public:
		auto operator()(const auto& context, const auto& accept) const -> decltype(auto) {
			if (local_key == napi_value{}) {
				local_key = ivm::napi::create_string_literal<Key>(context.env());
			}
			return accept(string_tag{}, local_key);
		}

	private:
		mutable napi_value local_key{};
};

// Napi function arguments to list
template <>
struct transferee_subject<napi_callback_info> : std::type_identity<napi_value> {};

template <class Meta>
struct visit<Meta, napi_callback_info> : visit<Meta, napi_value> {
		using visit<Meta, napi_value>::visit;
};

} // namespace ivm::value

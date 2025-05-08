module;
#include <utility>
#include <variant>
export module napi_js.visit;
import isolated_js;
import ivm.utility;
import napi_js.dictionary;
import napi_js.environment;
import napi_js.lock;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8;

namespace js {
using namespace napi;

// Delegate napi_value to various visitors
template <>
struct visit<void, napi_value>
		: napi::napi_isolate_witness_lock,
			visit<void, v8::Local<v8::External>> {
	public:
		using visit<void, v8::Local<v8::External>>::operator();

		visit(const environment& env, v8::Isolate* isolate) :
				napi_isolate_witness_lock{env, isolate} {}
		// nb: TODO: Remove (again)
		explicit visit(const environment& env) :
				visit{env, v8::Isolate::GetCurrent()} {}

		auto operator()(napi_value value, const auto_accept auto& accept) const -> decltype(auto) {
			switch (napi::invoke(napi_typeof, env(), value)) {
				case napi_boolean:
					return accept(boolean_tag{}, napi::bound_value{env(), napi::value<boolean_tag>::from(value)});
				case napi_number:
					return accept(number_tag{}, napi::bound_value{env(), napi::value<number_tag>::from(value)});
				case napi_bigint:
					return accept(bigint_tag{}, napi::bound_value{env(), napi::value<bigint_tag>::from(value)});
				case napi_string:
					return accept(string_tag{}, napi::bound_value{env(), napi::value<string_tag>::from(value)});
				case napi_object:
					{
						auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
						if (napi::invoke(napi_is_array, env(), value)) {
							// nb: It is intentional that `dictionary_tag` is bound. It handles sparse arrays.
							return accept(list_tag{}, napi::bound_value{env(), napi::value<dictionary_tag>::from(value)}, visit_entry);
						} else if (napi::invoke(napi_is_date, env(), value)) {
							return accept(date_tag{}, napi::bound_value{env(), napi::value<date_tag>::from(value)});
						} else if (napi::invoke(napi_is_promise, env(), value)) {
							return accept(promise_tag{}, value);
						}
						return accept(dictionary_tag{}, napi::bound_value{env(), napi::value<dictionary_tag>::from(value)}, visit_entry);
					}
				case napi_external:
					return (*this)(napi::to_v8(value).As<v8::External>(), accept);
				case napi_symbol:
					return accept(symbol_tag{}, value);
				case napi_null:
					return accept(null_tag{}, value);
				case napi_undefined:
					return accept(undefined_tag{}, std::monostate{});
				case napi_function:
					return accept(function_tag{}, value);
			}
		}
};

// Object key maker via napi
template <util::string_literal Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		struct visit {
			public:
				visit(const visit_key_literal& key_literal, const napi_witness_lock& lock) :
						local_key_{&key_literal.local_key_},
						lock_{&lock} {}

				[[nodiscard]] auto get() const -> napi_value {
					if (*local_key_ == napi_value{}) {
						*local_key_ = napi::invoke(napi_create_string_utf8, lock_->env(), Key.data(), Key.length());
					}
					return *local_key_;
				}

				auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept) const -> decltype(auto) {
					return accept(string_tag{}, get());
				}

			private:
				napi_value* local_key_;
				const napi_witness_lock* lock_;
		};
		friend visit;

	private:
		mutable napi_value local_key_{};
};

} // namespace js

module;
#include <string_view>
#include <utility>
export module napi_js.visit;
import isolated_js;
import ivm.utility;
import napi_js.bound_value;
import napi_js.dictionary;
import napi_js.environment;
import napi_js.primitive;
import napi_js.utility;
import napi_js.value;
import nodejs;
import v8;

namespace js {
using namespace napi;

// Napi visitor which can detect the underlying type of a given value
template <class Meta>
struct visit<Meta, napi_value>
		: visit<Meta, v8::Local<v8::External>>,
			napi::environment_scope<typename Meta::visit_context_type> {
	public:
		using visit<Meta, v8::Local<v8::External>>::operator();

		visit(auto* root, auto& env) :
				visit<Meta, v8::Local<v8::External>>{root},
				napi::environment_scope<typename Meta::visit_context_type>{env} {}

		template <class Tag>
		auto operator()(value<Tag> value, const auto& accept) const -> decltype(auto) {
			return accept(Tag{}, napi::bound_value{napi_env{*this}, value});
		}

		auto operator()(napi_value value, const auto& accept) const -> decltype(auto) {
			switch (napi::invoke(napi_typeof, napi_env{*this}, value)) {
				case napi_boolean:
					return (*this)(napi::value<boolean_tag>::from(value), accept);
				case napi_number:
					return (*this)(napi::value<number_tag>::from(value), accept);
				case napi_bigint:
					return (*this)(napi::value<bigint_tag>::from(value), accept);
				case napi_string:
					return (*this)(napi::value<string_tag>::from(value), accept);
				case napi_object:
					{
						auto visit_entry = std::pair<const visit&, const visit&>{*this, *this};
						if (napi::invoke(napi_is_array, napi_env{*this}, value)) {
							// nb: It is intentional that `dictionary_tag` is bound. It handles sparse arrays.
							return accept(list_tag{}, napi::bound_value{napi_env{*this}, napi::value<dictionary_tag>::from(value)}, visit_entry);
						} else if (napi::invoke(napi_is_date, napi_env{*this}, value)) {
							return (*this)(napi::value<date_tag>::from(value), accept);
						} else if (napi::invoke(napi_is_promise, napi_env{*this}, value)) {
							return accept(promise_tag{}, napi::value<promise_tag>::from(value));
						}
						return accept(dictionary_tag{}, napi::bound_value{napi_env{*this}, napi::value<dictionary_tag>::from(value)}, visit_entry);
					}
				case napi_external:
					return (*this)(napi::to_v8(value).As<v8::External>(), accept);
				case napi_symbol:
					return accept(symbol_tag{}, napi::value<symbol_tag>::from(value));
				case napi_null:
					return accept(null_tag{}, napi::value<null_tag>::from(value));
				case napi_undefined:
					return accept(undefined_tag{}, napi::value<undefined_tag>::from(value));
				case napi_function:
					return accept(function_tag{}, napi::value<function_tag>::from(value));
			}
		}
};

// Object key maker via napi
template <util::string_literal Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		[[nodiscard]] auto get_local(const auto& accept_or_visit) const -> napi_value {
			if (local_key_ == napi_value{}) {
				auto& environment = accept_or_visit.environment();
				auto& storage = environment.global_storage(value_literal<Key>{});
				if (storage) {
					local_key_ = storage.get(environment);
				} else {
					auto value = napi::value<string_tag>::make_property_name(environment, std::string_view{Key});
					storage.reset(environment, value);
					local_key_ = napi_value{value};
				}
			}
			return local_key_;
		}

		auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept) const -> decltype(auto) {
			return accept(string_tag{}, get_local(accept));
		}

	private:
		mutable napi_value local_key_{};
};

} // namespace js

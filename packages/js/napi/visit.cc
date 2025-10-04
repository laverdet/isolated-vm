module;
#include <string_view>
#include <utility>
export module napi_js:visit;
import :api;
import :bound_value;
import :dictionary;
import :environment;
import :external;
import :function;
import :primitive;
import :value;
import isolated_js;
import ivm.utility;

namespace js {
using namespace napi;

// Napi value visitor for property names
template <class Environment>
struct visit_napi_property : napi::environment_scope<Environment> {
	public:
		visit_napi_property(auto* /*transfer*/, Environment& env) :
				napi::environment_scope<Environment>{env} {}

		auto operator()(napi_value value, auto& accept) const -> decltype(auto) {
			auto typeof = napi::invoke(napi_typeof, napi_env{*this}, value);
			return accept_property_name(value, typeof, accept);
		}

		auto operator()(value<number_tag> value, auto& accept) const -> decltype(auto) {
			return accept_tagged(napi::value<number_tag_of<double>>::from(value), accept);
		}

		auto operator()(value<string_tag> value, auto& accept) const -> decltype(auto) {
			return accept_tagged(napi::value<string_tag_of<char16_t>>::from(value), accept);
		}

		auto operator()(value<symbol_tag> value, auto& accept) const -> decltype(auto) {
			return accept_tagged(value, accept);
		}

	protected:
		auto accept_property_name(napi_value value, napi_valuetype typeof, auto& accept) const -> decltype(auto) {
			switch (typeof) {
				case napi_number:
					return accept_tagged(napi::value<number_tag_of<double>>::from(value), accept);
				case napi_string:
					return accept_tagged(napi::value<string_tag_of<char16_t>>::from(value), accept);
				case napi_symbol:
					return accept_tagged(napi::value<symbol_tag>::from(value), accept);
				default:
					std::unreachable();
			}
		}

		template <auto_tag Tag>
		auto accept_tagged(value<Tag> value, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, Tag{}, *this, napi::bound_value{napi_env{*this}, value});
		}
};

// Napi visitor for all values
template <class Environment>
struct visit_napi_value : visit_napi_property<Environment> {
		using visit_property_type = visit_napi_property<Environment>;
		using visit_property_type::accept_property_name;
		using visit_property_type::accept_tagged;
		using visit_property_type::visit_napi_property;
		using visit_property_type::operator();

		auto operator()(napi_value value, auto& accept) const -> decltype(auto) {
			auto typeof = napi::invoke(napi_typeof, napi_env{*this}, value);
			switch (typeof) {
				case napi_number:
				case napi_string:
				case napi_symbol:
					return accept_property_name(value, typeof, accept);

				case napi_boolean:
					return accept_tagged(napi::value<boolean_tag>::from(value), accept);
				case napi_bigint:
					return accept_tagged(napi::value<bigint_tag_of<bigint>>::from(value), accept);
				case napi_object:
					{
						auto visit_entry = std::pair<const visit_property_type&, const visit_napi_value&>{*this, *this};
						if (napi::invoke(napi_is_array, napi_env{*this}, value)) {
							// nb: It is intentional that `dictionary_tag` is bound. It handles sparse arrays.
							return invoke_accept(accept, list_tag{}, visit_entry, napi::bound_value{napi_env{*this}, napi::value<dictionary_tag>::from(value)});
						} else if (napi::invoke(napi_is_date, napi_env{*this}, value)) {
							return accept_tagged(napi::value<date_tag>::from(value), accept);
						} else if (napi::invoke(napi_is_promise, napi_env{*this}, value)) {
							return accept_tagged(napi::value<promise_tag>::from(value), accept);
						}
						return invoke_accept(accept, dictionary_tag{}, visit_entry, napi::bound_value{napi_env{*this}, napi::value<dictionary_tag>::from(value)});
					}
				case napi_external:
					return accept_tagged(napi::value<external_tag>::from(value), accept);
				case napi_null:
					return accept_tagged(napi::value<null_tag>::from(value), accept);
				case napi_undefined:
					return accept_tagged(napi::value<undefined_tag>::from(value), accept);
				case napi_function:
					return accept_tagged(napi::value<function_tag>::from(value), accept);
			}
		}
};

// Napi value visitor entries
template <class Meta>
struct visit<Meta, napi_value> : visit_napi_value<typename Meta::visit_context_type> {
		using visit_napi_value<typename Meta::visit_context_type>::visit_napi_value;
};

// Object key maker via napi
template <util::string_literal Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		[[nodiscard]] auto get_local(const auto& accept_or_visit) const -> napi_value {
			if (local_key_ == napi_value{}) {
				auto& environment = accept_or_visit.environment();
				auto& storage = environment.global_storage(util::value_constant<Key>{});
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

		auto operator()(const auto& /*could_be_literally_anything*/, auto& accept) const -> decltype(auto) {
			return invoke_accept(accept, string_tag{}, *this, get_local(accept));
		}

	private:
		mutable napi_value local_key_{};
};

} // namespace js

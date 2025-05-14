module;
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
struct visit<Meta, napi_value> : visit<Meta, v8::Local<v8::External>> {
	public:
		using visit<Meta, v8::Local<v8::External>>::operator();

		explicit visit(auto_heritage auto visit_heritage, const auto& env) :
				visit<Meta, v8::Local<v8::External>>{visit_heritage(this)},
				env_{env} {}

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

		explicit operator napi_env() const {
			return napi_env{env_};
		}

	private:
		// NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
		const Meta::visit_context_type& env_;
};

// Object key maker via napi
template <util::string_literal Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		struct visit {
			public:
				constexpr visit(const visit_key_literal& key_literal, const auto& lock) :
						local_key_{&key_literal.local_key_},
						env_{napi_env{lock}} {}

				[[nodiscard]] auto get_local() const -> napi_value {
					if (*local_key_ == napi_value{}) {
						*local_key_ = napi::invoke(napi_create_string_utf8, env_, Key.data(), Key.length());
					}
					return *local_key_;
				}

				[[nodiscard]] constexpr auto get_utf8() const -> const char* {
					return Key.data();
				}

				auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept) const -> decltype(auto) {
					return accept(string_tag{}, get_local());
				}

			private:
				napi_value* local_key_;
				napi_env env_{};
		};
		friend visit;

	private:
		mutable napi_value local_key_{};
};

} // namespace js

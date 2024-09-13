module;
#include <cstring>
#include <utility>
#include <variant>
export module ivm.node:visit;
import :accept;
import :arguments;
import :environment;
import :object;
import ivm.v8;
import ivm.value;
import ivm.utility;
import napi;
import v8;

namespace ivm {

export template <class Type>
auto napi_to_v8(napi_value value) -> v8::Local<Type> {
	v8::Local<Type> local{};
	static_assert(std::is_pointer_v<napi_value>);
	static_assert(sizeof(void*) == sizeof(local));
	std::memcpy(&local, &value, sizeof(local));
	return local;
}

template <class Type>
auto napi_to_iv8(Napi::Value value) {
	auto& ienv = environment::get(value.Env());
	auto* isolate = ienv.isolate();
	auto context = ienv.context();
	return iv8::handle{napi_to_v8<Type>(value), {isolate, context}};
}

} // namespace ivm

namespace ivm::napi {

// Memoize `environment&` to avoid calling back into napi for each value
export class napi_callback_info_memo : non_copyable {
	public:
		napi_callback_info_memo(napi_callback_info_memo&&) = delete;
		~napi_callback_info_memo() = default;

		explicit napi_callback_info_memo(const Napi::CallbackInfo& info, environment& ienv) :
				info_{info},
				ienv_{&ienv} {}

		[[nodiscard]] auto ienv() const -> environment& {
			return *ienv_;
		}

		[[nodiscard]] auto napi_callback_info() const -> const Napi::CallbackInfo& {
			return info_;
		}

	private:
		const Napi::CallbackInfo& info_;
		environment* ienv_;
};

} // namespace ivm::napi

namespace ivm::value {

// Napi function arguments to list
template <>
struct visit<ivm::napi::napi_callback_info_memo> {
		auto operator()(const ivm::napi::napi_callback_info_memo& info, const auto& accept) const -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{info.napi_callback_info()});
		}
};

// Delegate Napi::Value to various visitors
template <>
struct visit<Napi::Value> {
		auto operator()(Napi::Value value, auto&& accept) const -> decltype(auto) {
			auto v8_common_delegate = [ & ]<class Type>(std::type_identity<Type> /*tag*/, Napi::Value value) -> decltype(auto) {
				return invoke_visit(napi_to_v8<Type>(value), std::forward<decltype(accept)>(accept));
			};
			switch (value.Type()) {
				case napi_boolean:
					return v8_common_delegate(std::type_identity<v8::Boolean>{}, value);
				case napi_number:
					return v8_common_delegate(std::type_identity<v8::Number>{}, value);
				case napi_string:
					return invoke_visit(napi_to_iv8<iv8::string>(value), std::forward<decltype(accept)>(accept));
				case napi_object:
					if (value.IsArray()) {
						return accept(list_tag{}, ivm::napi::object{value.As<Napi::Object>()});
					} else if (value.IsDate()) {
						return v8_common_delegate(std::type_identity<v8::Date>{}, value);
					} else if (value.IsPromise()) {
						return accept(promise_tag{}, value);
					}
					return accept(dictionary_tag{}, ivm::napi::object{value.As<Napi::Object>()});
				case napi_external:
					return v8_common_delegate(std::type_identity<v8::External>{}, value);
				case napi_symbol:
					return accept(symbol_tag{}, value);
				case napi_null:
					return accept(null_tag{}, std::nullptr_t{});
				case napi_undefined:
					return accept(undefined_tag{}, std::monostate{});
				case napi_bigint:
				case napi_function:
					return accept(value_tag{}, std::type_identity<void>{});
			}
		}
};

} // namespace ivm::value

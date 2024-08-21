module;
#include <cstring>
#include <utility>
export module ivm.node:visit;
import :arguments;
import :environment;
import ivm.v8;
import ivm.value;
import ivm.utility;
import napi;
import v8;

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

// Delegate Napi::Value to iv8 common acceptor
template <class Meta>
struct accept<Meta, Napi::Value> : non_copyable {
		accept() = delete;
		accept(const accept&) = delete;
		accept(accept&&) = delete;
		~accept() = default;
		auto operator=(const accept&) -> accept& = delete;
		auto operator=(accept&&) -> accept& = delete;

		explicit accept(Napi::Env env, environment& ienv) :
				env_{env},
				ienv_{&ienv} {}

		// nb: This handles `direct_cast` from a v8 handle to napi handle
		template <class Type>
		auto operator()(value_tag /*tag*/, v8::Local<Type> local) const -> Napi::Value {
			napi_value holder{};
			std::memcpy(&holder, &local, sizeof(local));
			return {env_, holder};
		}

		auto operator()(auto_tag auto tag, auto&& value) const -> Napi::Value {
			auto local = delegate_accept<v8::Local<v8::Value>>(
				*this,
				tag,
				std::forward<decltype(value)>(value),
				ienv_->isolate(),
				ienv_->context()
			);
			return (*this)(value_tag{}, local);
		}

	private:
		Napi::Env env_;
		environment* ienv_;
};

// Napi function arguments to list
template <>
struct visit<ivm::napi::napi_callback_info_memo> {
		auto operator()(const ivm::napi::napi_callback_info_memo& info, const auto& accept) const -> decltype(auto) {
			return accept(vector_tag{}, ivm::napi::arguments{info.napi_callback_info()});
		}
};

// Delegate Napi::Value to iv8 common visitor
template <>
struct visit<Napi::Value> {
		auto operator()(Napi::Value value, auto&& accept) const -> decltype(auto) {
			v8::Local<v8::Value> local{};
			napi_value holder{value};
			std::memcpy(&local, &holder, sizeof(local));
			auto& ienv = environment::get(value.Env());
			auto* isolate = ienv.isolate();
			auto context = ienv.context();
			return invoke_visit(
				iv8::handle{local, {isolate, context}},
				std::forward<decltype(accept)>(accept)
			);
		}
};

} // namespace ivm::value

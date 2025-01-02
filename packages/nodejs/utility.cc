module;
#include <cstring>
#include <expected>
#include <span>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>
export module ivm.node:utility;
import :environment;
import ivm.js;
import ivm.napi;
import ivm.utility;
import napi;

namespace ivm {

export using expected_value = std::expected<napi_value, napi_value>;

export class handle_scope : util::non_moveable {
	public:
		explicit handle_scope(napi_env env) :
				env_{env},
				handle_scope_{js::napi::invoke(napi_open_handle_scope, env)} {}

		~handle_scope() {
			js::napi::invoke_dtor(napi_close_handle_scope, env_, handle_scope_);
		}

	private:
		napi_env env_;
		napi_handle_scope handle_scope_;
};

export class reference : util::non_copyable {
	public:
		reference(napi_env env, napi_value value) :
				env_{env},
				value_{js::napi::invoke(napi_create_reference, env, value, 1)} {}

		reference(reference&& right) noexcept :
				env_{right.env_},
				value_{std::exchange(right.value_, nullptr)} {}

		~reference() {
			if (value_ != nullptr) {
				js::napi::invoke_dtor(napi_delete_reference, env_, value_);
			}
		}

		auto operator=(reference&& right) = delete;

		auto operator*() const -> napi_value {
			return js::napi::invoke(napi_get_reference_value, env_, value_);
		}

	private:
		napi_env env_;
		napi_ref value_;
};

auto make_promise(environment& ienv, auto accept) {
	auto* env = ienv.nenv();

	// nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	auto* promise = js::napi::invoke(napi_create_promise, env, &deferred);

	// nodejs promise fulfillment
	ienv.scheduler().increment_ref();
	auto dispatch =
		[ accept = std::move(accept),
			deferred,
			env,
			scheduler = ienv.scheduler() ](
			auto&&... args
		) mutable {
			scheduler.schedule(
				[ accept = std::move(accept),
					deferred,
					env,
					... args = std::forward<decltype(args)>(args) ]() mutable {
					auto scope = handle_scope{env};
					auto& ienv = environment::get(env);
					ienv.scheduler().decrement_ref();
					if (auto result = accept(ienv, std::forward<decltype(args)>(args)...)) {
						js::napi::invoke0(napi_resolve_deferred, env, deferred, result.value());
					} else {
						js::napi::invoke0(napi_reject_deferred, env, deferred, result.error());
					}
				}
			);
		};

	// `[ dispatch, promise ]`
	return std::make_tuple(std::move(dispatch), promise);
}

template <class Tuple>
struct callback_parameters;

template <class... Types>
struct callback_parameters<std::tuple<environment&, Types...>>
		: std::type_identity<std::tuple<Types...>> {};

export template <auto Fn>
auto make_node_function(environment& ienv) -> napi_value {
	using parameters_type = callback_parameters<util::functor_parameters_t<decltype(Fn)>>::type;
	auto* env = ienv.nenv();
	auto callback = [](napi_env env, napi_callback_info info) -> napi_value {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		std::array<napi_value, 8> arguments;
		auto count = arguments.size();
		// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
		void* data;
		js::napi::invoke0(napi_get_cb_info, env, info, &count, arguments.data(), nullptr, &data);
		if (count > arguments.size()) {
			throw std::runtime_error{"Too many arguments"};
		}
		auto& ienv = *static_cast<environment*>(data);
		return std::apply(
			Fn,
			std::tuple_cat(
				std::forward_as_tuple(ienv),
				js::transfer<parameters_type>(std::span{arguments}.subspan(0, count), std::tuple{env, ienv.isolate()}, std::tuple{})
			)
		);
	};
	return js::napi::invoke(napi_create_function, env, nullptr, 0, callback, &ienv);
}

} // namespace ivm

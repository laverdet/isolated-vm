export module napi_js:promise;
import :value;
import std;

namespace js::napi {

// Resolver for `make_promise`
template <class Environment, class Dispatch>
class resolver {
	public:
		resolver(Environment& env, napi_deferred deferred, Dispatch dispatch = {}) :
				env_{env},
				deferred_{deferred},
				scheduler_{env.scheduler()},
				dispatch_{dispatch} {
			scheduler_.increment_ref();
		}
		resolver(const resolver&) = delete;
		resolver(resolver&&) = default;
		~resolver() {
			if (scheduler_) {
				resolve(std::monostate{});
			}
		}

		auto operator=(const resolver&) -> resolver& = delete;
		auto operator=(resolver&&) -> resolver& = default;

		// Fulfill using callback supplied to constructor
		auto operator()(auto&&... args) -> void
			requires std::invocable<Dispatch&, Environment&, decltype(args)...> {
			using result_type = std::invoke_result_t<Dispatch&, Environment&, decltype(args)...>;
			using expected_type = std::expected<result_type, js::error_value>;
			schedule(
				[](Environment& environment, auto dispatch, auto&&... args) -> expected_type {
					return expected_type{std::in_place, std::move(dispatch)(environment, std::forward<decltype(args)>(args)...)};
				},
				std::move(dispatch_),
				std::forward<decltype(args)>(args)...
			);
		}

		// Fulfill using direct resolution value. Note it still needs try/catch (in `fulfill`) because
		// the `transfer` operation could fail.
		auto resolve(auto value) -> void {
			using expected_type = std::expected<decltype(value), js::error_value>;
			schedule(
				[](Environment& /*env*/, auto value) -> expected_type {
					return expected_type{std::in_place, std::move(value)};
				},
				std::move(value)
			);
		}

		// Fulfill using thrown error.
		auto reject(js::error_value error) -> void {
			using expected_type = std::expected<std::monostate, js::error_value>;
			schedule(
				[](Environment& /*env*/, js::error_value error) -> expected_type {
					return expected_type{std::unexpect, std::move(error)};
				},
				std::move(error)
			);
		}

	private:
		auto schedule(auto operation, auto&&... args) -> void {
			auto scheduler = std::move(scheduler_);
			scheduler(
				[](napi_env env, napi_deferred deferred, auto operation, auto&&... args) {
					auto& environment = napi::environment::unsafe_get_environment_as<Environment>(env);
					environment.scheduler().decrement_ref();
					const auto scope = js::napi::handle_scope{env};
					try {
						auto expected = operation(environment, std::forward<decltype(args)>(args)...);
						if (expected) {
							auto value = js::transfer_in_strict<napi_value>(*std::move(expected), environment);
							napi::invoke0(napi_resolve_deferred, env, deferred, value);
						} else {
							auto error = js::transfer_in_strict<napi_value>(std::move(expected).error(), environment);
							napi::invoke0(napi_reject_deferred, env, deferred, error);
						}
					} catch (const napi::pending_error& /*error*/) {
						auto* exception = napi::invoke(napi_get_and_clear_last_exception, env);
						napi::invoke0(napi_reject_deferred, env, deferred, exception);
					} catch (const js::error& error) {
						auto exception = js::transfer_in_strict<napi_value>(error, environment);
						napi::invoke0(napi_reject_deferred, env, deferred, exception);
					}
				},
				env_,
				deferred_,
				std::move(operation),
				std::forward<decltype(args)>(args)...
			);
		}

		napi_env env_;
		napi_deferred deferred_;
		uv_scheduler scheduler_;
		Dispatch dispatch_;
};

template <class Environment>
resolver(Environment&, napi_deferred) -> resolver<Environment, std::monostate>;

export template <class Environment>
auto make_promise(Environment& env, auto... dispatch) {
	// Make nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_value promise;
	napi::invoke0(napi_create_promise, napi_env{env}, &deferred, &promise);

	// Make resolve helper
	auto resolve = resolver{env, deferred, std::move(dispatch)...};

	// `[ promise, resolver ]`
	return std::tuple{value_of<promise_tag>::from(promise), std::move(resolve)};
}

} // namespace js::napi

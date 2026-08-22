export module napi_js:promise;
import :lock;
import :value;
import std;

namespace js::napi {

// Resolver for `make_promise`
template <class Environment, class Dispatch>
class resolver {
	public:
		using lock_type = environment_lock_witness_of<Environment>;

		resolver(const lock_type& lock, napi_deferred deferred, Dispatch dispatch = {}) :
				env_{lock},
				deferred_{deferred},
				scheduler_{lock->scheduler()},
				dispatch_{dispatch} {
			scheduler_.increment_ref(napi_env{lock});
		}
		resolver(const resolver&) = delete;
		resolver(resolver&&) = default;
		~resolver() {
			if (scheduler_) {
				resolve(nullptr);
			}
		}

		auto operator=(const resolver&) -> resolver& = delete;
		auto operator=(resolver&&) -> resolver& = default;

		// Fulfill using callback supplied to constructor
		auto operator()(auto&&... args) -> void
			requires std::invocable<Dispatch&, const lock_type&, decltype(args)...> {
			using result_type = std::invoke_result_t<Dispatch&, const lock_type&, decltype(args)...>;
			using expected_type = std::expected<result_type, js::error_value>;
			schedule(
				[](const lock_type& lock, auto dispatch, auto&&... args) -> expected_type {
					return expected_type{std::in_place, std::move(dispatch)(lock, std::forward<decltype(args)>(args)...)};
				},
				std::move(dispatch_),
				std::forward<decltype(args)>(args)...
			);
		}

		// Fulfill using direct resolution value. Note it still needs try/catch (in `fulfill`) because
		// the `transfer` operation could fail.
		template <class Type>
		auto resolve(Type value) noexcept(std::is_nothrow_move_constructible_v<Type>) -> void {
			using expected_type = std::expected<decltype(value), js::error_value>;
			schedule(
				[](const lock_type& /*lock*/, auto value) -> expected_type {
					return expected_type{std::in_place, std::move(value)};
				},
				std::move(value)
			);
		}

		// Fulfill using thrown error.
		auto reject(js::error_value error) noexcept -> void {
			using expected_type = std::expected<std::monostate, js::error_value>;
			schedule(
				[](const lock_type& /*lock*/, js::error_value error) -> expected_type {
					return expected_type{std::unexpect, std::move(error)};
				},
				std::move(error)
			);
		}

	private:
		template <class Operation, class... Args>
		auto schedule(Operation operation, Args&&... args) noexcept(std::is_nothrow_move_constructible_v<Operation> && (std::is_nothrow_move_constructible_v<Args> && ...)) -> void {
			auto scheduler = std::move(scheduler_);
			scheduler(
				[](napi_env env, napi_value /*nothing*/, napi_deferred deferred, auto operation, auto&&... args) noexcept {
					auto& environment = napi::environment::unsafe_get_environment_as<Environment>(env);
					environment.scheduler().decrement_ref(env);
					auto lock = lock_type{environment_lock_witness::make_witness(environment), environment};
					auto try_catch = environment_try_catch{lock};
					try {
						auto expected = operation(lock, std::forward<decltype(args)>(args)...);
						if (expected) {
							auto value = js::transfer_in_strict<napi_value>(*std::move(expected), lock);
							napi::invoke0(napi_resolve_deferred, env, deferred, value);
						} else {
							auto error = js::transfer_in_strict<napi_value>(std::move(expected).error(), lock);
							napi::invoke0(napi_reject_deferred, env, deferred, error);
						}
					} catch (const napi::pending_error& /*error*/) {
						auto* exception = napi::invoke(napi_get_and_clear_last_exception, env);
						napi::invoke0_noexcept(napi_reject_deferred, env, deferred, exception);
					} catch (const napi::pending_v8_error& /*error*/) {
						napi::invoke0_noexcept(napi_reject_deferred, env, deferred, try_catch.take_exception());
					} catch (const js::error& error) {
						auto exception = js::transfer_in_strict<napi_value>(error, lock);
						napi::invoke0_noexcept(napi_reject_deferred, env, deferred, exception);
					}
				},
				deferred_,
				std::move(operation),
				std::forward<decltype(args)>(args)...
			);
		}

		napi_env env_;
		napi_deferred deferred_;
		napi_scheduler scheduler_;
		Dispatch dispatch_;
};

template <class Environment>
resolver(const environment_lock_witness_of<Environment>&, napi_deferred) -> resolver<Environment, std::monostate>;

template <class Environment, class Dispatch>
resolver(const environment_lock_witness_of<Environment>&, napi_deferred, Dispatch) -> resolver<Environment, Dispatch>;

export template <class Environment>
auto make_promise(const environment_lock_witness_of<Environment>& lock, auto... dispatch) {
	// Make nodejs promise & future
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_deferred deferred;
	// NOLINTNEXTLINE(cppcoreguidelines-init-variables)
	napi_value promise;
	napi::invoke0(napi_create_promise, napi_env{lock}, &deferred, &promise);

	// Make resolve helper
	auto resolve = resolver{lock, deferred, std::move(dispatch)...};

	// `[ promise, resolver ]`
	return std::tuple{local_of<promise_tag>::from(promise), std::move(resolve)};
}

} // namespace js::napi

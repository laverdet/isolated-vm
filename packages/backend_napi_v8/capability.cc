module backend_napi_v8;
import :capability;
import :environment;
import :lock;
import :reference;
import :utility;
import auto_js;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// Similar to `std::promise` / `std::future` except an abandoned promise resolves `std::nullopt`.
class capability_handle::sync_instance::result {
	public:
		using value_type = std::expected<js::value_t, js::error_value>;
		class future;
		class promise;

		static auto make() -> std::tuple<promise, future>;

	private:
		util::lockable_with<std::optional<value_type>, {.interuptable = true, .notifiable = true}> state_;
};

// Host side
class capability_handle::sync_instance::result::promise : util::non_copyable {
	public:
		promise(const promise&) = delete;
		promise(promise&&) = default;
		~promise() {
			if (state_) {
				set_value(value_type{std::in_place, nullptr});
			}
		}
		auto operator=(const promise&) -> promise& = delete;
		auto operator=(promise&&) noexcept -> promise& = default;

		// Settling more than once is UB
		auto set_value(value_type value) -> void {
			std::exchange(state_, nullptr)->state_.write_notify()->emplace(std::move(value));
		}

	private:
		friend class result;
		explicit promise(std::shared_ptr<result> state) : state_{std::move(state)} {}

		std::shared_ptr<result> state_;
};

// Isolate side
class capability_handle::sync_instance::result::future : util::non_copyable {
	public:
		// Blocks until the value is settled. The result is empty if `stop_token` is stopped first.
		auto get(util::inplace_stop_token stop_token) -> std::optional<value_type> {
			auto lock = state_->state_.write_waitable(&std::optional<value_type>::has_value);
			lock.wait(stop_token);
			return std::move(*lock);
		}

	private:
		friend class result;
		explicit future(std::shared_ptr<result> state) : state_{std::move(state)} {}

		std::shared_ptr<result> state_;
};

auto capability_handle::sync_instance::result::make() -> std::tuple<promise, future> {
	auto state = std::make_shared<result>();
	return {promise{state}, future{std::move(state)}};
}

// `capability_handle::sync_instance`
capability_handle::sync_instance::sync_instance(const environment::lock& lock, js::napi::local_of<js::function_tag> callback) :
		scheduler_{lock->scheduler().make_ref(lock)},
		callback_{js::napi::make_unique_remote(lock, callback)} {}

auto capability_handle::sync_instance::instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> {
	auto capability =
		[ self = std::move(*this) ](
			const realm_scope& lock,
			js::rest /*rest*/,
			js::forward<js::iv8::callback_info, js::vector_tag> arguments
		) -> js::forward<v8::Local<v8::Value>> {
		auto [ promise, future ] = sync_instance::result::make();
		(*self.scheduler_)(
			// Invoked in the node thread. Invokes `callback(...params)` and settles `promise` through one
			// of the two functions.
			[](
				napi_env nenv,
				napi_value /*nothing*/,
				sync_instance::result::promise promise,
				const js::napi::remote<js::function_tag>& callback,
				js::values_vector_t params
			) -> void {
				auto& host_environment = napi::environment::unsafe_get_environment_as<environment>(nenv);
				auto lock = environment::lock{napi::environment_lock_witness::make_witness(host_environment), host_environment};
				// `resolve` & `reject` share ownership of the promise
				const auto shared = std::make_shared<sync_instance::result::promise>(std::move(promise));
				auto dispatched = napi::invoke_externalized_error_scope(lock, [ & ] -> void {
					auto resolve = js::free_function{
						[ shared ](const environment::lock& lock, js::forward<js::napi::local_of<>> value) -> void {
							shared->set_value(napi::invoke_externalized_error_scope(lock, [ & ] -> js::value_t {
								return transfer_from_host<js::value_t>(lock, *value);
							}));
						},
					};
					auto reject = js::free_function{
						[ shared ](const environment::lock& lock, js::forward<js::napi::local_of<>> reason) -> void {
							shared->set_value(std::unexpected{napi::externalize_caught_error(lock, *reason)});
						},
					};
					auto params_value = js::napi::local_of<js::list_tag>::from(js::transfer_in_strict<napi_value>(std::move(params), lock, accept_references));
					auto [ resolve_value, reject_value ] = [ & ] -> auto {
						using transfer_type = std::array<js::napi::local_of<js::function_tag>, 2>;
						auto subject = [ & ] -> auto { return std::forward_as_tuple(std::move(resolve), std::move(reject)); };
						return js::transfer_in_strict<transfer_type>(subject(), lock);
					}();
					callback.deref(lock)->invoke_async(lock, params_value, resolve_value, reject_value);
				});
				if (!dispatched) {
					shared->set_value(std::unexpected{std::move(dispatched).error()});
				}
			},
			std::move(promise),
			std::cref(*self.callback_),
			transfer_from_runtime<js::values_vector_t>(lock, *arguments)
		);
		// Block until the host settles, or the agent is terminated.
		auto& scheduler = lock->storage()->foreground_runner().scheduler();
		auto value = scheduler.suspend([ & ] -> auto {
			auto suspension = execution_timeout_suspension{};
			auto unlock = js::iv8::isolated::executor::unlock{util::slice(lock), lock->executor()};
			return future.get(lock->executor().stop_token());
		});
		if (value) {
			if (*value) {
				// Resolved
				return js::forward{js::transfer_in_strict<v8::Local<v8::Value>>(**std::move(value), lock, accept_references)};
			} else {
				// Rejected
				lock.isolate()->ThrowException(js::transfer_in_strict<v8::Local<v8::Value>>(std::move(value)->error(), lock));
				throw js::iv8::pending_error{};
			}
		} else {
			// Abandoned
			return js::forward{js::transfer_in_strict<v8::Local<v8::Value>>(nullptr, lock)};
		}
	};
	return js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{std::move(capability)}, lock);
}

// `capability_handle::void_instance`
capability_handle::void_instance::void_instance(const environment::lock& lock, js::napi::local_of<js::function_tag> callback) :
		scheduler_{lock->scheduler().make_ref(lock)},
		callback_{js::napi::make_shared_remote(lock, callback)} {}

auto capability_handle::void_instance::instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> {
	auto capability =
		[ self = std::move(*this) ](
			const realm_scope& lock,
			js::rest /*rest*/,
			js::forward<js::iv8::callback_info, js::vector_tag> arguments
		) -> void {
		(*self.scheduler_)(
			[](
				napi_env nenv,
				napi_value /*nothing*/,
				js::napi::shared_remote<js::function_tag> callback,
				js::values_vector_t params
			) -> void {
				auto& host_environment = napi::environment::unsafe_get_environment_as<environment>(nenv);
				auto lock = environment::lock{napi::environment_lock_witness::make_witness(host_environment), host_environment};
				napi::invoke_internal_error_scope(lock, [ & ] -> void {
					auto argv = js::transfer_in_strict<std::vector<napi_value>>(std::move(params), lock, accept_references);
					callback->deref(lock)->invoke(lock, argv);
				});
			},
			self.callback_,
			transfer_from_runtime<js::values_vector_t>(lock, *arguments)
		);
	};
	return js::transfer_in<v8::Local<v8::FunctionTemplate>>(js::free_function{std::move(capability)}, lock);
}

// `capability_handle`
auto capability_handle::sync(const environment::lock& lock, forward_callback_type callback) -> forward_capability_type {
	auto make = [ & ] -> auto { return capability_handle{instance_type{std::type_identity<sync_instance>{}, lock, *callback}}; };
	return js::forward{class_template(lock)->construct(lock, make())};
}

auto capability_handle::void_(const environment::lock& lock, forward_callback_type callback) -> forward_capability_type {
	auto make = [ & ] -> auto { return capability_handle{instance_type{std::type_identity<void_instance>{}, lock, *callback}}; };
	return js::forward{class_template(lock)->construct(lock, make())};
}

auto capability_handle::class_template(const environment::lock& lock) -> js::napi::local_of<js::class_tag_of<capability_handle>> {
	return lock->class_template(
		std::type_identity<capability_handle>{},
		lock,
		js::class_template{
			js::class_constructor{util::cw<"Capability">},
			js::class_static{util::cw<"sync">, &sync},
			js::class_static{util::cw<"void">, &void_},
		}
	);
}

} // namespace backend_napi_v8

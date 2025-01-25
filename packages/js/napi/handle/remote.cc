module;
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>
export module napi_js.remote;
import ivm.utility;
import napi_js.environment;
import napi_js.handle_scope;
import napi_js.utility;
import napi_js.uv_scheduler;
import napi_js.value;
import napi_js.value.internal;
import nodejs;

namespace js::napi {

template <class Type>
concept remote_handle_environment =
	std::is_base_of_v<environment, Type> &&
	std::is_base_of_v<uv_schedulable, Type>;

// Thread safe persistent value reference
export template <class Tag>
class remote : private reference_handle {
	private:
		struct private_ctor {};
		static auto expire(remote* ptr) -> void;

	public:
		using unique_remote = std::unique_ptr<remote, util::function_type_of<expire>>;

		remote(private_ctor /*private*/, napi_env env, napi_value value, uv_scheduler scheduler) :
				reference_handle{env, value},
				scheduler_{std::move(scheduler)} {}

		auto get(const environment& env) const -> value<Tag>;

		static auto make_shared(remote_handle_environment auto& env, value<Tag> value) -> std::shared_ptr<remote>;
		static auto make_unique(remote_handle_environment auto& env, value<Tag> value) -> unique_remote;

	private:
		uv_scheduler scheduler_;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = remote<Type>::unique_remote;

export template <class Tag>
auto make_shared_remote(remote_handle_environment auto& env, value<Tag> value) -> shared_remote<Tag> {
	return remote<Tag>::make_shared(env, value);
}

export template <class Tag>
auto make_unique_remote(remote_handle_environment auto& env, value<Tag> value) -> unique_remote<Tag> {
	return remote<Tag>::make_unique(env, value);
}

// ---

template <class Tag>
auto remote<Tag>::expire(remote* ptr) -> void {
	std::unique_ptr<remote> self{ptr};
	ptr->scheduler_.schedule([ self = std::move(self) ] mutable {
		handle_scope scope{self->env()};
		self.reset();
	});
}

template <class Tag>
auto remote<Tag>::get(const environment& env) const -> value<Tag> {
	return value<Tag>::from(reference_handle::get(env));
}

template <class Tag>
auto remote<Tag>::make_shared(remote_handle_environment auto& env, value<Tag> value) -> std::shared_ptr<remote> {
	return std::shared_ptr<remote>{new remote{private_ctor{}, env, value, env.scheduler()}, expire};
}

template <class Tag>
auto remote<Tag>::make_unique(remote_handle_environment auto& env, value<Tag> value) -> unique_remote {
	return unique_remote{new remote{private_ctor{}, env, value, env.scheduler()}};
}

} // namespace js::napi

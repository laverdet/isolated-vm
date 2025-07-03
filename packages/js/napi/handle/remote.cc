module;
#include <cassert>
#include <memory>
#include <type_traits>
#include <utility>
export module napi_js:remote;
import :environment;
import :handle_scope;
import :reference;
import :uv_scheduler;
import :value;
import ivm.utility;
import nodejs;

namespace js::napi {

template <class Type>
concept remote_handle_environment =
	std::is_base_of_v<environment, Type> &&
	std::is_base_of_v<uv_schedulable, Type>;

// Thread safe persistent value reference
export template <class Tag>
class remote : protected detail::reference_handle {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};
		static auto expire(remote* ptr) -> void;

	public:
		using unique_remote = std::unique_ptr<remote, util::function_type_of<expire>>;

		remote(private_constructor /*private*/, const napi::environment& env, value<Tag> value, uv_scheduler scheduler) :
				reference_handle{napi_env{env}, napi_value{value}},
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
	ptr->scheduler_(
		util::make_indirect_moveable_function(
			[ self = std::move(self) ] mutable {
				handle_scope scope{self->env()};
				self.reset();
			}
		)
	);
}

template <class Tag>
auto remote<Tag>::get(const environment& env) const -> value<Tag> {
	return value<Tag>::from(get_value(napi_env{env}));
}

template <class Tag>
auto remote<Tag>::make_shared(remote_handle_environment auto& env, value<Tag> value) -> std::shared_ptr<remote> {
	return std::shared_ptr<remote>{new remote{private_constructor{}, env, value, env.scheduler()}, expire};
}

template <class Tag>
auto remote<Tag>::make_unique(remote_handle_environment auto& env, value<Tag> value) -> unique_remote {
	return unique_remote{new remote{private_constructor{}, env, value, env.scheduler()}};
}

} // namespace js::napi

export module napi_js:remote;
import :environment;
import :lock;
import :reference;
import std;
import util;

namespace js::napi {

template <class Type>
concept remote_handle_environment =
	std::is_base_of_v<environment, Type> &&
	std::is_base_of_v<napi_schedulable, Type>;

// Thread safe persistent value reference
export template <class Tag>
class remote : protected reference_handle {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};
		static auto expire(remote* ptr) -> void;

	public:
		using unique_remote = std::unique_ptr<remote, util::function_constant<expire>>;

		remote() = default;
		remote(private_constructor /*private*/, environment_lock_witness lock, local_of<Tag> value, napi_scheduler scheduler) :
				reference_handle{napi_env{lock}, napi_value{value}},
				scheduler_{std::move(scheduler)} {}

		auto deref(environment_lock_witness lock) const -> local_of<Tag>;

		template <remote_handle_environment Environment>
		static auto make_shared(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> std::shared_ptr<remote>;
		template <remote_handle_environment Environment>
		static auto make_unique(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> unique_remote;

	private:
		napi_scheduler scheduler_;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = remote<Type>::unique_remote;

export template <remote_handle_environment Environment, class Tag>
auto make_shared_remote(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> shared_remote<Tag> {
	return remote<Tag>::make_shared(lock, value);
}

export template <remote_handle_environment Environment, class Tag>
auto make_unique_remote(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> unique_remote<Tag> {
	return remote<Tag>::make_unique(lock, value);
}

// ---

template <class Tag>
auto remote<Tag>::expire(remote* ptr) -> void {
	std::unique_ptr<remote> self{ptr};
	ptr->scheduler_(
		[](napi_env /*env*/, napi_value /*nothing*/, auto self) -> void {
			self.reset();
		},
		std::move(self)
	);
}

template <class Tag>
auto remote<Tag>::deref(environment_lock_witness lock) const -> local_of<Tag> {
	return local_of<Tag>::from(get_value(napi_env{lock}));
}

template <class Tag>
template <remote_handle_environment Environment>
auto remote<Tag>::make_shared(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> std::shared_ptr<remote> {
	return std::shared_ptr<remote>{new remote{private_constructor{}, lock, value, lock->scheduler()}, expire};
}

template <class Tag>
template <remote_handle_environment Environment>
auto remote<Tag>::make_unique(const environment_lock_witness_of<Environment>& lock, local_of<Tag> value) -> unique_remote {
	return unique_remote{new remote{private_constructor{}, lock, value, lock->scheduler()}};
}

} // namespace js::napi

module;
#include <boost/intrusive/list.hpp>
#include <cstring>
#include <memory>
export module ivm.isolated_v8:remote;
import :agent_fwd;
import isolated_v8.scheduler;
import v8;

namespace isolated_v8 {

using intrusive_no_size = boost::intrusive::constant_time_size<false>;
using intrusive_normal_mode = boost::intrusive::link_mode<boost::intrusive::normal_link>;
using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_normal_mode>;

// Base class for `remote<T>`. Performs insertion and removal from the agent storage, and the
// underlying v8 handle.
class remote_handle : util::non_moveable {
	public:
		remote_handle(agent::lock& agent_lock, v8::Local<v8::Data> handle);
		~remote_handle();

		auto deref(agent::lock& agent_lock) -> v8::Local<v8::Data>;
		auto reset(agent::lock& agent_lock) -> void;

	private:
		std::shared_ptr<agent::storage> agent_storage_;
		v8::Global<v8::Data> global_;
		intrusive_list_hook hook_;

	public:
		using hook_type = boost::intrusive::member_hook<remote_handle, intrusive_list_hook, &remote_handle::hook_>;
};

// Reference to a persistent in v8. It should always be created with `remote<T>::make_shared` or
// `make_unique`. When the smart pointer is released the handle schedules a task to reset itself in
// the isolate thread.
export template <class Type>
class remote : remote_handle {
	private:
		struct private_ctor {
				explicit private_ctor() = default;
		};
		static auto send(remote* self) -> void;

	public:
		using unique_remote = std::unique_ptr<remote, util::functor_of<send>>;
		remote(private_ctor /*private*/, agent::lock& agent_lock, v8::Local<v8::Data> handle) :
				remote_handle{agent_lock, handle} {}

		auto deref(agent::lock& agent_lock) -> v8::Local<Type>;
		static auto make_shared(agent::lock& agent_lock, v8::Local<Type> handle) -> std::shared_ptr<remote>;
		static auto make_unique(agent::lock& agent_lock, v8::Local<Type> handle) -> unique_remote;
};

// Non-owning intrusive list of all outstanding remote handles. Before destruction of the isolate
// all handles can be reset.
class remote_handle_list {
	public:
		auto erase(remote_handle& handle) -> void;
		auto insert(remote_handle& handle) -> void;
		auto reset(agent::lock& lock) -> void;

	private:
		using list_type = boost::intrusive::list<remote_handle, intrusive_no_size, remote_handle::hook_type>;
		using lockable_list_type = util::lockable<list_type>;
		lockable_list_type list_;
};

// Convenience helpers
export template <class Type>
using shared_remote = std::shared_ptr<remote<Type>>;

export template <class Type>
using unique_remote = remote<Type>::unique_remote;

export template <class Type>
auto make_shared_remote(agent::lock& agent, v8::Local<Type> handle) -> shared_remote<Type> {
	return remote<Type>::make_shared(agent, handle);
}

// ---

template <class Type>
auto remote<Type>::deref(agent::lock& agent_lock) -> v8::Local<Type> {
	// `handle.As<Type>()` doesn't work because handles like `UnboundScript` don't provide a `Cast`
	// function even though they are `Data`.
	auto data = remote_handle::deref(agent_lock);
	v8::Local<Type> local;
	static_assert(sizeof(data) == sizeof(local));
	std::memcpy(&local, static_cast<void*>(&data), sizeof(local));
	return local;
}

template <class Type>
auto remote<Type>::make_shared(agent::lock& agent_lock, v8::Local<Type> handle) -> std::shared_ptr<remote> {
	return std::shared_ptr<remote>{new remote{private_ctor{}, agent_lock, handle}, send};
}

template <class Type>
auto remote<Type>::make_unique(agent::lock& agent_lock, v8::Local<Type> handle) -> unique_remote {
	return unique_remote{new remote{private_ctor{}, agent_lock, handle}};
}

template <class Type>
auto remote<Type>::send(remote* self) -> void {
	std::unique_ptr<remote_handle> ptr{self};
	// TODO: Break the remote -> agent -> remote cycle
}

} // namespace isolated_v8

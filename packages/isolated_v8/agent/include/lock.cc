module;
#include <memory>
export module isolated_v8.agent:lock;
import isolated_v8.lock;
import isolated_v8.remote_handle;
import :fwd;

namespace isolated_v8 {

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock final
		: public util::pointer_facade<agent::lock>,
			public isolate_scope_lock,
			public remote_handle_lock {
	public:
		explicit lock(std::shared_ptr<agent::host> host);

		auto operator*(this auto& self) -> decltype(auto) { return *self.host_; }
		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;

	private:
		std::shared_ptr<host> host_;
};

// This keeps the `weak_ptr` in `agent` alive. The `host` maintains a `weak_ptr` to this and can
// "sever" the client connection if it needs to.
class agent::severable {
	public:
		explicit severable(std::shared_ptr<host> host);

		auto sever() -> void;

	private:
		std::shared_ptr<host> host_;
};

} // namespace isolated_v8

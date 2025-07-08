module;
#include <memory>
export module isolated_v8:agent_lock;
import :agent_fwd;
import :remote_handle;
import ivm.utility;
import v8_js;

namespace isolated_v8 {

// A `lock` is a simple holder for an `agent::host` which proves that we are executing in
// the isolate context.
class agent::lock final
		: util::non_moveable,
			public util::pointer_facade,
			public js::iv8::isolate_managed_lock,
			public remote_handle_lock {
	public:
		explicit lock(std::shared_ptr<agent::host> host);
		~lock();

		auto operator->(this auto& self) -> auto* { return self.host_.get(); }
		auto accept_remote_handle(remote_handle& remote) noexcept -> void final;
		[[nodiscard]] auto remote_expiration_task() const -> reset_handle_type final;
		static auto get_current() -> lock&;

	private:
		std::shared_ptr<host> host_;
		lock* previous_;
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

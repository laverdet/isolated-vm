module;
#include <memory>
#include <optional>
export module isolated_v8:agent_host;
import :clock;
import :cluster;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export class agent_host;

export struct behavior_params {
		clock::any_clock clock;
		std::optional<double> random_seed;
};

// This keeps the `weak_ptr` in `agent_handle` alive. The `agent_host` maintains a `weak_ptr` to
// this and can "sever" the client connection if it needs to.
// TODO: This could be handled with an internal counter on the `agent_host` instead of creating a
// separate shared_ptr control block.
export class agent_severable {
	public:
		explicit agent_severable(std::shared_ptr<agent_host> host);
		auto sever() -> void;

	private:
		std::atomic<std::shared_ptr<agent_host>> host_;
};

namespace {
// Return a bound member function for `remote_handle_lock`.
constexpr auto make_remote_expiration_callback = []<class Type>(Type* host) -> auto {
	return util::bind{util::fn<&Type::remote_expiration_callback>, std::ref(*host)};
};
} // namespace

// Directly handles the actual isolate. If someone has a reference to this then it probably means
// the isolate is locked and entered.
export class agent_host : public std::enable_shared_from_this<agent_host> {
	private:
		template <class Type> friend class agent_host_of;

		struct random_seed_unlatch : util::non_copyable {
				explicit random_seed_unlatch(bool& latch);
				auto operator()() const -> void;
				bool* latch;
		};

	public:
		explicit agent_host(
			cluster& cluster,
			std::shared_ptr<js::iv8::platform::foreground_runner> foreground_runner,
			behavior_params params
		);
		// This should be protected but that breaks `std::make_shared`. `destroy_isolate_callback` must
		// be called before destruction but this invariant isn't enforced statically.
		~agent_host() = default;

		auto autorelease_pool() -> util::autorelease_pool& { return autorelease_pool_; }
		auto clock(this auto& self) -> auto& { return self.clock_; }
		auto clock_time_ms() -> int64_t;
		auto foreground_runner(this auto& self) -> auto& { return self.foreground_runner_; }
		auto isolate() -> v8::Isolate* { return isolate_.get(); }
		auto random_seed_latch() -> util::scope_exit<random_seed_unlatch>;
		auto remote_expiration_callback(js::iv8::expired_remote_type remote) noexcept -> void;
		auto remote_handle_lock(js::iv8::isolate_lock_witness witness) -> js::iv8::remote_handle_lock;
		auto remote_handle_list() -> auto& { return remote_handle_list_; }
		auto scratch_context() -> v8::Local<v8::Context>;
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;

		static auto acquire_severable(const std::shared_ptr<agent_host>& self) -> std::shared_ptr<agent_severable>;
		static auto get_current() -> agent_host*;
		static auto get_current(v8::Isolate* isolate) -> agent_host& { return *static_cast<agent_host*>(isolate->GetData(0)); }

	protected:
		auto destroy_isolate_callback(auto locked_callback) -> void;

	private:
		using reset_handle_callback_type = std::invoke_result_t<decltype(make_remote_expiration_callback), agent_host*>;

		auto reset_remote_handle(js::iv8::expired_remote_type remote) -> void;
		static auto dispose_isolate(v8::Isolate* isolate) -> void { isolate->Dispose(); }

		std::reference_wrapper<cluster> cluster_;
		util::autorelease_pool autorelease_pool_;
		std::weak_ptr<agent_severable> severable_;
		std::shared_ptr<js::iv8::platform::foreground_runner> foreground_runner_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, util::function_constant<dispose_isolate>> isolate_;
		js::iv8::remote_handle_list remote_handle_list_;
		reset_handle_callback_type reset_handle_callback_{make_remote_expiration_callback(this)};
		js::iv8::reset_handle_type reset_handle_{reset_handle_callback_};
		v8::Global<v8::Context> scratch_context_;

		bool should_give_seed_{false};
		std::optional<double> random_seed_;
		clock::any_clock clock_;
};

// Agent which also manages environment storage of a given type.
export template <class Type>
class agent_host_of;

template <class Type>
class agent_host_of : public agent_host {
	public:
		explicit agent_host_of(auto&&... creation_args)
			requires std::constructible_from<agent_host, decltype(creation_args)...> :
				agent_host{std::forward<decltype(creation_args)>(creation_args)...} {}
		agent_host_of(const agent_host_of&) = delete;
		agent_host_of(agent_host_of&&) = delete;
		~agent_host_of();
		auto operator=(const agent_host_of&) -> agent_host_of& = delete;
		auto operator=(agent_host_of&&) -> agent_host_of& = delete;

		auto environment(this auto& self) -> auto& { return *self.environment_; }
		auto initialize_environment(auto make_environment) -> void;

	private:
		std::optional<Type> environment_;
};

// ---

auto agent_host::destroy_isolate_callback(auto locked_callback) -> void {
	cluster_.get().release_runner(foreground_runner_);
	foreground_runner_->terminate();
	foreground_runner_->finalize();
	auto lock = js::iv8::isolate_execution_lock{isolate_.get()};
	autorelease_pool_.clear();
	remote_handle_list_.clear(util::slice{lock});
	locked_callback(lock);
}

template <class Type>
agent_host_of<Type>::~agent_host_of() {
	destroy_isolate_callback([ this ](const js::iv8::isolate_execution_lock& /*lock*/) -> void {
		environment_.reset();
	});
}

template <class Type>
auto agent_host_of<Type>::initialize_environment(auto make_environment) -> void {
	environment_.emplace(make_environment());
}

} // namespace isolated_v8

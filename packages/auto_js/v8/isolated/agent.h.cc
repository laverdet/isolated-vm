module;
#include <boost/intrusive/list.hpp>
#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <variant>
export module v8_js:isolated.agent;
import :collected_handle;
import :isolated.clock;
import :lock;
import :platform.foreground_runner;
import :remote;

namespace js::iv8::isolated {

// Forward declarations
export class cluster;
export class agent_host;
export template <class Type>
class agent_host_of;

// `agent_host` behavior parameters
export struct behavior_params {
		clock::any_clock clock;
		std::optional<double> random_seed;
};

// `isolate_lock_witness_of` adapters
class agent_remote_handle_lock : public remote_handle_lock {
	public:
		agent_remote_handle_lock(isolate_lock_witness lock, agent_host& agent);
};

class agent_collected_handle_lock : public collected_handle_lock {
	public:
		agent_collected_handle_lock(isolate_lock_witness lock, agent_host& agent);
};

// An `agent_lock` is a simple holder for an `agent_host` which proves that we are executing in the
// isolate context.
export using agent_lock = isolate_lock_witness_of<agent_host, agent_remote_handle_lock, agent_collected_handle_lock>;
export template <class Environment>
using agent_lock_of = isolate_lock_witness_of<agent_host_of<Environment>, agent_remote_handle_lock, agent_collected_handle_lock>;
export using realm_scope = context_lock_witness_of<agent_host, agent_remote_handle_lock, agent_collected_handle_lock>;
export template <class Environment>
using realm_scope_of = context_lock_witness_of<agent_host_of<Environment>, agent_remote_handle_lock, agent_collected_handle_lock>;

// Manages storage which must outlive the `agent_host`
export class agent_storage {
	public:
		explicit agent_storage(cluster& cluster) : cluster_{cluster} {}
		auto foreground_runner() -> platform::foreground_runner& { return foreground_runner_; }
		static auto release(std::shared_ptr<agent_storage> storage) -> void;

	private:
		using intrusive_safe_mode = boost::intrusive::link_mode<boost::intrusive::safe_link>;
		using intrusive_list_hook = boost::intrusive::list_member_hook<intrusive_safe_mode>;

		intrusive_list_hook list_hook_;
		std::reference_wrapper<cluster> cluster_;
		platform::foreground_runner foreground_runner_;

	public:
		using intrusive_hook = boost::intrusive::member_hook<agent_storage, intrusive_list_hook, &agent_storage::list_hook_>;
};

// Holds a `std::weak_ptr` to an `agent_host`. On creation / destruction of the handle modifies the
// reference count in `agent_host`. When the reference count reaches zero the agent will release the
// handle it holds to itself. This scheme allows the host to sever all handles by simply releasing
// its own handle.
export class agent_handle {
	public:
		using lock = agent_lock;
		explicit agent_handle(std::shared_ptr<agent_host> host);
		agent_handle(const agent_handle&);
		agent_handle(agent_handle&&) = default;
		~agent_handle();
		auto operator=(const agent_handle&) -> agent_handle& = delete;
		auto operator=(agent_handle&&) -> agent_handle& = default;

	protected:
		[[nodiscard]] auto lock_host() const -> std::shared_ptr<agent_host> { return host_.lock(); }

	private:
		std::weak_ptr<agent_host> host_;
		bool weak_{};
};

export template <class Type>
class agent_handle_of : public agent_handle {
	public:
		using lock = agent_lock_of<Type>;

		explicit agent_handle_of(const std::shared_ptr<agent_host_of<Type>>& host) : agent_handle{host} {}

		template <class... Args, std::invocable<lock, Args...> Task>
		auto schedule(Task task, Args... args) const -> void;
};

// Generic client storage, stored along with `agent_host`. If a value is stored (via `emplace`) then
// `destroy` must be explicitly invoked before the destructor.
template <class Type>
class agent_host_environment : util::non_copyable {
	public:
		agent_host_environment() = default;
		~agent_host_environment();

		agent_host_environment(const agent_host_environment&) = delete;
		auto operator=(const agent_host_environment&) -> agent_host_environment& = delete;

		auto destroy() noexcept -> void { instance_.reset(); }
		auto emplace(std::convertible_to<Type> auto instance) -> void { instance_.emplace(std::move(instance)); }
		auto operator*() -> auto& { return *instance_; }
		auto operator*() const -> auto& { return *instance_; }

	private:
		std::optional<Type> instance_;
};

// Directly manages an isolate. If someone has a reference to this then it probably means the
// isolate is locked and entered.
export class agent_host
		: util::non_moveable,
			public std::enable_shared_from_this<agent_host> {
	public:
		using destroy_callback_type = util::function_ref<auto(isolate_lock_witness) noexcept -> void>;
		explicit agent_host(destroy_callback_type destroy_callback, std::shared_ptr<agent_storage> storage, behavior_params params);
		~agent_host();

		auto autorelease_pool() -> util::autorelease_pool& { return autorelease_pool_; }
		auto clock(this auto& self) -> auto& { return self.clock_; }
		auto isolate() -> v8::Isolate* { return isolate_.get(); }
		auto make_context() -> v8::Local<v8::Context>;
		auto make_remote_handle_lock(isolate_lock_witness lock) -> remote_handle_lock;
		auto scratch_context() -> v8::Local<v8::Context>;
		auto storage() -> auto& { return storage_; }
		auto take_random_seed() -> std::optional<double>;
		auto task_runner(v8::TaskPriority priority) -> std::shared_ptr<v8::TaskRunner>;

		static auto get_current(v8::Isolate* isolate) -> agent_host&;

	private:
		friend class agent_handle;
		using lockable_handle_type = util::lockable_with<std::shared_ptr<agent_host>, {.shared = true}>;

		auto remote_expiration_callback(expired_remote_type remote) noexcept -> void;
		static auto dispose_isolate(v8::Isolate* isolate) -> void { isolate->Dispose(); }

		// Order matters for these members
		std::shared_ptr<agent_storage> storage_;
		std::unique_ptr<v8::ArrayBuffer::Allocator> array_buffer_allocator_;
		std::unique_ptr<v8::Isolate, util::function_constant<dispose_isolate>> isolate_;

		// Order doesn't really matter
		bool should_give_seed_{};
		clock::any_clock clock_;
		destroy_callback_type destroy_callback_;
		lockable_handle_type self_handle_;
		remote_handle_list remote_handle_list_;
		reset_handle_type reset_handle_callback_;
		std::atomic<unsigned> handle_count_;
		std::optional<double> random_seed_;
		util::autorelease_pool autorelease_pool_;
		v8::Global<v8::Context> scratch_context_;
};

// Host which also manages client storage
export template <class Type>
class agent_host_of
		: private agent_host_environment<Type>,
			public agent_host {
	public:
		explicit agent_host_of(auto&&... args)
			requires std::constructible_from<agent_host, destroy_callback_type, decltype(args)...> :
				agent_host{
					destroy_callback_type{util::fn<&agent_host_of::destroy_callback>, *this},
					std::forward<decltype(args)>(args)...
				} {}

		auto emplace_environment(std::convertible_to<Type> auto environment) -> void { agent_host_environment<Type>::emplace(std::move(environment)); }
		auto environment(this auto& self) -> auto& { return *self; }

	private:
		auto destroy_callback(isolate_lock_witness /*lock*/) noexcept -> void { agent_host_environment<Type>::destroy(); }
};

// ---

template <class Type>
template <class... Args, std::invocable<typename agent_handle_of<Type>::lock, Args...> Task>
auto agent_handle_of<Type>::schedule(Task task, Args... args) const -> void {
	if (auto host = lock_host()) {
		auto& scheduler = host->storage()->foreground_runner();
		scheduler.schedule_client_task(
			[](std::stop_token /*stop_token*/, auto host, auto task, auto... args) -> void {
				auto isolate_lock = isolate_execution_lock{host->isolate()};
				std::visit([](auto& clock) -> void { clock.begin_tick(); }, host->clock());
				task(lock{isolate_lock, static_cast<agent_host_of<Type>&>(*host)}, std::move(args)...);
			},
			std::move(host),
			std::move(task),
			std::move(args)...
		);
	}
}

template <class Type>
agent_host_environment<Type>::~agent_host_environment() {
	assert(!instance_);
	if (instance_) {
		std::unreachable();
	}
}

} // namespace js::iv8::isolated

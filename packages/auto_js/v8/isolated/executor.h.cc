export module v8_js:isolated.executor;
import :lock;
import auto_js;
import std;
import util;
import v8;

namespace js::iv8::isolated {

export class executor : util::non_moveable { // "En taro adun" lmao
	public:
		class timeout;
		class unlock;
		auto initialize(std::invocable<v8::Isolate*> auto initialize) -> void;
		auto reset() -> void;
		auto terminate() -> void;

		auto exchange_timeout(timeout* timeout) noexcept -> void;
		[[nodiscard]] auto is_terminated() const -> bool;
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_.get(); }
		[[nodiscard]] auto stop_token() const -> util::inplace_stop_token { return stop_source_.get_token(); }

	private:
		constexpr static auto dispose_isolate = [](v8::Isolate* isolate) -> void { isolate->Dispose(); };
		static auto on_before_call_entered(v8::Isolate* isolate) -> void;

		std::unique_ptr<v8::Isolate, decltype(dispose_isolate)> isolate_;
		util::inplace_stop_source stop_source_;
		timeout* current_timeout_ = nullptr;
};

class executor::unlock : util::non_moveable {
	public:
		explicit unlock(isolate_lock_witness lock, executor& executor);
		~unlock();

	private:
		isolate_unlock unlock_;
		std::reference_wrapper<executor> executor_;
};

// Execution time budget.
class executor::timeout : util::non_moveable {
	public:
		class scope;
		class suspension;
		using duration_type = util::timer_stop_token::duration_type;

		timeout(executor& executor, duration_type duration);
		~timeout();

		// nb: Estimate only
		[[nodiscard]] auto remaining() const -> duration_type { return timer_.remaining(); }
		auto adjust(duration_type duration) -> bool { return timer_.adjust(duration); }
		auto set(duration_type duration) -> bool { return timer_.set(duration); }

	private:
		friend executor;
		auto activate() noexcept -> void;
		auto deactivate() noexcept -> void;

		// Invoked from the timer thread.
		class expire {
			public:
				explicit expire(executor& executor) : executor_{&executor} {}
				auto operator()() const noexcept -> void;

			private:
				static auto interrupt(v8::Isolate* isolate, void* data) -> void;
				executor* executor_;
		};

		std::reference_wrapper<executor> executor_;
		util::timer_stop_token timer_;
		std::optional<util::inplace_stop_callback<expire>> callback_;
};

// Makes `timeout` the running budget for the duration of the scope, then restores the previous one.
class executor::timeout::scope : util::non_moveable {
	public:
		scope(executor& executor, timeout* timeout);
		~scope();

	private:
		std::reference_wrapper<executor> executor_;
		timeout* previous_;
};

// Pauses the running budget, if any, for the duration of the scope.
class executor::timeout::suspension : scope {
	public:
		explicit suspension(executor& executor) : scope{executor, nullptr} {}
};

// ---

auto executor::initialize(std::invocable<v8::Isolate*> auto initialize) -> void {
	static_assert(std::is_nothrow_invocable_v<decltype(initialize), v8::Isolate*>);
	isolate_.reset(v8::Isolate::Allocate());
	auto params = v8::Isolate::CreateParams{initialize(isolate_.get())};
	v8::Isolate::Initialize(isolate_.get(), params);
	isolate_->AddBeforeCallEnteredCallback(on_before_call_entered);
}

} // namespace js::iv8::isolated

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
		class timeout_suspension;
		class unlock;
		auto initialize(std::invocable<v8::Isolate*> auto initialize) -> void;
		auto reset() -> void;
		auto terminate() -> void;

		[[nodiscard]] auto is_terminated() const -> bool;
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_.get(); }
		[[nodiscard]] auto stop_token() const -> util::inplace_stop_token { return stop_source_.get_token(); }

	private:
		constexpr static auto dispose_isolate = [](v8::Isolate* isolate) -> void { isolate->Dispose(); };
		static auto on_before_call_entered(v8::Isolate* isolate) -> void;

		std::unique_ptr<v8::Isolate, decltype(dispose_isolate)> isolate_;
		util::inplace_stop_source stop_source_;
		timeout* current_timeout_{};
};

class executor::unlock : util::non_moveable {
	public:
		explicit unlock(isolate_lock_witness lock, executor& executor);
		~unlock();

	private:
		isolate_unlock unlock_;
		std::reference_wrapper<executor> executor_;
};

// Requests termination after the given duration.
class executor::timeout : util::non_moveable {
	public:
		timeout(executor& executor, js_clock::duration duration);
		~timeout();

		auto pause() -> void { timer_.pause(); }
		auto resume() -> void { timer_.resume(); }

	private:
		class expire {
			public:
				explicit expire(v8::Isolate* isolate) : isolate_{isolate} {}
				auto operator()() const noexcept -> void;

			private:
				v8::Isolate* isolate_;
		};

		std::reference_wrapper<executor> executor_;
		util::timer_stop_token timer_;
		util::scope_stack<timeout*> scope_;
		std::optional<util::inplace_stop_callback<expire>> callback_;
};

// Pauses the current timeout, if any, for the duration of the scope.
class executor::timeout_suspension : util::non_moveable {
	public:
		explicit timeout_suspension(executor& executor);
		~timeout_suspension();

	private:
		timeout* timeout_;
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

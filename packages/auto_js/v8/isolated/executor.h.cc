export module v8_js:isolated.executor;
import std;
import v8;

namespace js::iv8::isolated {

export class executor { // "En taro adun"
	public:
		auto initialize(std::invocable<v8::Isolate*> auto initialize) -> void;
		auto reset() -> void;
		auto terminate() -> void;

		[[nodiscard]] auto is_terminated() const -> bool;
		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_.get(); }

	private:
		constexpr static auto dispose_isolate = [](v8::Isolate* isolate) -> void { isolate->Dispose(); };
		static auto on_before_call_entered(v8::Isolate* isolate) -> void;

		std::unique_ptr<v8::Isolate, decltype(dispose_isolate)> isolate_;
		std::atomic<bool> terminated_;
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

export module v8_js:callback_storage;
import :collected_handle;
import :unmaybe;
import std;
import v8;

namespace js::iv8 {

// Converts any invocable into a `v8::FunctionCallback` and `v8::Value` (data value). The data value
// may have an attached weak ref callback. This is structured a bit differently from the napi
// version, because in pure v8 it makes more sense to attach the "finalizer" to the function data
// value, instead of the function itself. Also, napi ensures that callback are invoked under the
// same thread, but we don't have the same assurance here.
template <class Function, class Agent, class... Implements>
auto make_callback_storage(
	util::type_pack<Agent, Implements...> /*types*/,
	isolate_lock_witness lock,
	const collected_handle_lock& handle_lock,
	Agent& agent,
	Function function
) {
	if constexpr (std::is_trivially_copyable_v<Function>) {
		static_assert(std::is_trivially_destructible_v<Function>);
		if constexpr (std::is_empty_v<Function>) {
			// Constant expression function of 0 size. `Data()` is the agent.
			auto data = v8::External::New(lock.isolate(), &agent);
			const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				auto invoke = Function{};
				auto witness = context_lock_witness::from_isolate(isolate_lock_witness::make_witness(info.GetIsolate()));
				auto lock = revive_lock_of<Agent, Implements...>(witness, info.Data().As<v8::External>()->Value());
				invoke(lock, info);
			}};
			return std::tuple{callback, data};
		} else {
			// Trivial data-only function type. `Data()` is a latin1 string containing the agent & function data.
			auto trampoline = util::bind{
				[](Agent& agent, Function& invoke, v8::Isolate* isolate, const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
					auto witness = context_lock_witness::from_isolate(isolate_lock_witness::make_witness(isolate));
					auto lock = revive_lock_of<Agent, Implements...>(witness, &agent);
					invoke(lock, info);
				},
				std::ref(agent),
				function,
			};
			using trampoline_type = decltype(trampoline);
			auto data =
				unmaybe(v8::String::NewFromOneByte(lock.isolate(), reinterpret_cast<const std::uint8_t*>(&trampoline), v8::NewStringType::kNormal, sizeof(trampoline)));
			const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
				std::array<std::byte, sizeof(trampoline_type)> data;
				auto* isolate = info.GetIsolate();
				info.Data().As<v8::String>()->WriteOneByteV2(isolate, 0, data.size(), reinterpret_cast<std::uint8_t*>(data.data()), v8::String::WriteFlags::kNone);
				std::bit_cast<trampoline_type>(data)(isolate, info);
			}};
			return std::tuple{callback, data};
		}
	} else {
		// Otherwise the function requires bound state w/ a destructor. This call is a separate overload
		// so that trivial functions may use a plain isolate_lock_witness.
		using pair_type = std::pair<Agent*, Function>;
		auto data = make_collected_external<pair_type>(handle_lock, &agent, std::move(function));
		const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto& state = unwrap_collected_external<pair_type>(info.Data().As<v8::External>());
			auto witness = context_lock_witness::from_isolate(isolate_lock_witness::make_witness(info.GetIsolate()));
			auto lock = revive_lock_of<Agent, Implements...>(witness, state.first);
			state.second(lock, info);
		}};
		return std::tuple{callback, data};
	}
}

template <class Agent, class... Implements>
auto make_callback_storage(const isolate_lock_witness_of<Agent, Implements...>& lock, auto function) {
	return make_callback_storage(util::type_pack<Agent, Implements...>{}, lock, lock, *lock, std::move(function));
}

template <class Agent, class... Implements>
auto make_callback_storage(const context_lock_witness_of<Agent, Implements...>& lock, auto function) {
	return make_callback_storage(util::type_pack<Agent, Implements...>{}, lock, lock, *lock, std::move(function));
}

// Callback storage maker for stateless isolate contexts. These may not be collected objects.
template <class Function>
auto make_callback_storage(isolate_lock_witness lock, Function function) {
	static_assert(std::is_trivially_copyable_v<Function>);
	static_assert(std::is_trivially_destructible_v<Function>);
	if constexpr (std::is_empty_v<Function>) {
		// Constant expression function of 0 size. No data needed at all!
		auto data = v8::Local<v8::Data>{};
		const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto invoke = Function{};
			auto witness = context_lock_witness::from_isolate(isolate_lock_witness::make_witness(info.GetIsolate()));
			invoke(witness, info);
		}};
		return std::tuple{callback, data};
	} else if constexpr (sizeof(Function) == sizeof(void*)) {
		// Trivial function of pointer type. Data() is the function data.
		auto data = v8::External::New(lock.isolate(), std::bit_cast<void*>(function));
		const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto& invoke = *static_cast<Function*>(info.Data().As<v8::External>()->Value());
			auto witness = context_lock_witness::from_isolate(isolate_lock_witness::make_witness(info.GetIsolate()));
			invoke(witness, info);
		}};
		return std::tuple{callback, data};
	}
}

} // namespace js::iv8

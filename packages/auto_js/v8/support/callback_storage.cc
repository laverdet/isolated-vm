module;
#include <bit>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
export module v8_js:callback_storage;
import :collected_handle;
import :lock;
import :unmaybe;
import v8;

namespace js::iv8 {

template <class Agent, class... Implements>
auto revive_lock_of(v8::Isolate* isolate, void* agent) -> context_lock_witness_of<Agent, Implements...> {
	auto isolate_witness = isolate_lock_witness::make_witness(isolate);
	auto context_witness = context_lock_witness::make_witness(isolate_witness, isolate_witness.isolate()->GetCurrentContext());
	return context_lock_witness_of<Agent, Implements...>{context_witness, *static_cast<Agent*>(agent)};
}

// Converts any invocable into a `v8::FunctionCallback` and `v8::Value` (data value). The data value
// may have an attached weak ref callback. This is structured a bit differently from the napi
// version, because in pure v8 it makes more sense to attach the "finalizer" to the function data
// value, instead of the function itself. Also, napi ensures that callback are invoked under the
// same thread, but we don't have the same assurance here.
template <class Agent, class... Implements>
auto make_callback_storage(const isolate_lock_witness_of<Agent, Implements...>& lock, auto function) {
	using function_type = decltype(function);
	if constexpr (std::is_trivially_copyable_v<function_type>) {
		static_assert(std::is_trivially_destructible_v<function_type>);
		if constexpr (std::is_empty_v<function_type>) {
			// Constant expression function of 0 size. `Data()` is the agent.
			auto external_data = v8::External::New(lock.isolate(), std::addressof(*lock));
			const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				auto invoke = function_type{};
				auto lock = revive_lock_of<Agent, Implements...>(info.GetIsolate(), info.Data().As<v8::External>()->Value());
				invoke(lock, info);
			}};
			return std::tuple{callback, external_data};
		} else {
			// Trivial data-only function type. `Data()` is a latin1 string containing the agent & function data.
			auto trampoline = util::bind{
				[](Agent& agent, function_type& invoke, v8::Isolate* isolate, const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
					auto lock = revive_lock_of<Agent, Implements...>(isolate, &agent);
					invoke(lock, info);
				},
				std::ref(*lock),
				function,
			};
			using trampoline_type = decltype(trampoline);
			auto string_data =
				unmaybe(v8::String::NewFromOneByte(lock.isolate(), reinterpret_cast<const uint8_t*>(&trampoline), v8::NewStringType::kNormal, sizeof(trampoline)));
			const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				std::array<std::byte, sizeof(trampoline_type)> data;
				auto* isolate = info.GetIsolate();
				info.Data().As<v8::String>()->WriteOneByteV2(isolate, 0, data.size(), reinterpret_cast<uint8_t*>(data.data()), v8::String::WriteFlags::kNone);
				std::bit_cast<trampoline_type>(data)(isolate, info);
			}};
			return std::tuple{callback, string_data};
		}
	} else {
		// Otherwise the function requires bound state w/ a destructor
		using pair_type = std::pair<Agent*, function_type>;
		auto external = make_collected_external<pair_type>(lock, std::addressof(*lock), std::move(function));
		const auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto& state = unwrap_collected_external<pair_type>(info.Data().As<v8::External>());
			auto lock = revive_lock_of<Agent, Implements...>(info.GetIsolate(), state.first);
			state.second(lock, info);
		}};
		return std::tuple{callback, external};
	}
}

} // namespace js::iv8

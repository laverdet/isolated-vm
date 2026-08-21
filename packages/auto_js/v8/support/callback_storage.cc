export module v8_js:callback_storage;
import :external;
import :unmaybe;
import std;
import v8;

namespace js::iv8 {

// callback which does not require an agent pointer
constexpr auto make_plain_callback =
	[]<class Lock>(
		std::type_identity<auto(Lock, const v8::FunctionCallbackInfo<v8::Value>&)->void> /*signature*/,
		const auto& /*lock*/,
		auto function
	) -> auto {
	return util::bind{
		[](auto& function, const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			function(context_lock_witness{info}, info);
		},
		std::move(function),
	};
};

// callback which requires an agent pointer
constexpr auto make_decorated_callback =
	[]<class Agent, class... Implements>(
		std::type_identity<auto(const context_lock_witness_of<Agent, Implements...>&, const v8::FunctionCallbackInfo<v8::Value>&)->void> /*signature*/,
		const auto& lock,
		auto function
	) -> auto {
	return util::bind{
		[](auto& agent, auto& function, const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto witness = context_lock_witness{info};
			auto decorated_witness = context_lock_witness_of<Agent, Implements...>{witness, agent};
			function(decorated_witness, info);
		},
		std::ref(*lock),
		std::move(function),
	};
};

// returns a `v8::FunctionCallback`-ish callback
constexpr auto make_bound_callback = util::overloaded{
	make_plain_callback,
	[]<class Agent, class... Implements>(
		std::type_identity<auto(const isolate_lock_witness_of<Agent, Implements...>&, const v8::FunctionCallbackInfo<v8::Value>&)->void> signature,
		const auto& lock,
		auto function
	) -> auto {
		return make_decorated_callback(signature, lock, std::move(function));
	},
	[]<class Agent, class... Implements>(
		std::type_identity<auto(const context_lock_witness_of<Agent, Implements...>&, const v8::FunctionCallbackInfo<v8::Value>&)->void> signature,
		const auto& lock,
		auto function
	) -> auto {
		return make_decorated_callback(signature, lock, std::move(function));
	},
};

// Converts any invocable into a `v8::FunctionCallback` and `v8::Value` (data value). The data value
// may have an attached weak ref callback. This is structured a bit differently from the napi
// version, because in pure v8 it makes more sense to attach the "finalizer" to the function data
// value, instead of the function itself. Also, napi ensures that callback are invoked under the
// same thread, but we don't have the same assurance here.
auto make_callback_storage(const auto& lock, auto function) {
	using signature_type = util::function_signature_t<decltype(function)>;
	auto bound = make_bound_callback(std::type_identity<signature_type>{}, lock, std::move(function));
	using bound_type = decltype(bound);
	if constexpr (std::is_trivially_copyable_v<bound_type>) {
		if constexpr (std::is_empty_v<bound_type>) {
			// Constant expression function of 0 size. No data needed at all!
			auto data = v8::Local<v8::Value>{};
			constexpr auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				auto invoke = bound_type{};
				invoke(info);
			}};
			return std::tuple{callback, data};
		} else {
			if constexpr (sizeof(bound_type) <= sizeof(void*)) {
				// Trivial function which can fit inside a pointer. There is a v8 DCHECK on nullptr, so in
				// that case it falls through to the latin1 representation.
				if (auto* pointer = util::bit_padding_cast<void*>(bound); pointer != nullptr) {
					using external_type = untagged_external<void>;
					auto data = v8::Local<v8::Value>{external_type::make(lock, pointer)};
					constexpr auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
						auto invoke = util::bit_truncation_cast<bound_type>(info.Data().As<external_type>()->value());
						invoke(info);
					}};
					return std::tuple{callback, data};
				}
			}
			// Trivial data-only function type. `Data()` is a latin1 string containing the function data.
			auto data =
				unmaybe(v8::String::NewFromOneByte(lock.isolate(), reinterpret_cast<const std::uint8_t*>(&bound), v8::NewStringType::kNormal, sizeof(bound)));
			constexpr auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
				// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
				std::array<std::byte, sizeof(bound_type)> data;
				auto* isolate = info.GetIsolate();
				info.Data().As<v8::String>()->WriteOneByteV2(isolate, 0, data.size(), reinterpret_cast<std::uint8_t*>(data.data()), v8::String::WriteFlags::kNone);
				auto invoke = std::bit_cast<bound_type>(data);
				invoke(info);
			}};
			return std::tuple{callback, v8::Local<v8::Value>{data}};
		}
	} else {
		// Otherwise `Data()` is a collected external.
		using external_type = untagged_external<bound_type>;
		auto data = external_type::make_collected(lock, std::move(bound));
		constexpr auto callback = v8::FunctionCallback{[](const v8::FunctionCallbackInfo<v8::Value>& info) -> void {
			auto& invoke = *info.Data().As<external_type>()->value();
			invoke(info);
		}};
		return std::tuple{callback, data};
	}
}

} // namespace js::iv8

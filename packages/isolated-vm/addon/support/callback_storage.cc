module;
#include "auto_js/no_unique_address.h"
export module isolated_vm:support.callback_storage;
export import :support.callback_info_fwd;
import :support.lock_fwd;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

const auto max_callback_storage_size = 32;

using runtime_callback_type = auto (*)(const runtime_lock&, callback_info, const void*) -> value_of<>;
using runtime_callback_finalizer_type = auto (*)(void*) -> void;

using runtime_callback_data_span_type = std::span<std::byte>;
using runtime_callback_data_allocated_type = std::unique_ptr<void, runtime_callback_finalizer_type>;

// Runtime host requires extra pointers for storage, which is allocated upfront in the addon.
struct runtime_callback_data_storage {
		runtime_callback_data_storage() = default;
		void* data;
		runtime_callback_type callback;
};

template <class Type>
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
struct runtime_callback_function_storage : runtime_callback_data_storage {
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		explicit runtime_callback_function_storage(Type function) : function{std::move(function)} {}
		runtime_callback_function_storage() = default;
		NO_UNIQUE_ADDRESS Type function;
};

// Make callback storage for an addon function. It returns `std::pair<callback_type, data_type>`
// where `data_type` is either `void*` for trivial word-sized functions, `std::array<std::byte, N>`
// for larger trivial functions, or `std::unique_ptr<T>` for non-trivial functions with allocated
// data.
auto make_callback_storage(std::invocable<const runtime_lock&, callback_info> auto function) {
	using function_type = decltype(function);
	using data_type = runtime_callback_function_storage<function_type>;
	if constexpr (std::is_trivially_copyable_v<function_type>) {
		// Trivial function type. `data` is `std::array<std::byte, N>`.
		static_assert(sizeof(function_type) <= max_callback_storage_size);
		auto storage = data_type{std::move(function)};
		storage.callback = runtime_callback_type{[](const runtime_lock& lock, callback_info info, const void* data) -> value_of<> {
			const auto& storage = *static_cast<const data_type*>(data);
			return storage.function(lock, info);
		}};
		return storage;
	} else {
		// Otherwise the function requires bound state w/ a destructor
		using data_type = runtime_callback_function_storage<function_type>;
		auto storage = std::make_unique<data_type>(std::move(function));
		storage->callback = runtime_callback_type{[](const runtime_lock& lock, callback_info info, const void* data) -> value_of<> {
			const auto& storage = *static_cast<const data_type*>(data);
			return storage.function(lock, info);
		}};
		return runtime_callback_data_allocated_type{
			storage.release(),
			[](void* data) { delete static_cast<data_type*>(data); },
		};
	}
}

} // namespace isolated_vm

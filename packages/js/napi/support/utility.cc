module;
#include <functional>
export module napi_js:utility;
import nodejs;

namespace js::napi {

// See: `v8_enable_direct_handle`.

// Null handle for both direct and indirect handles
const void* null_value_handle_ = nullptr;
export auto* const null_value_handle = reinterpret_cast<napi_value>(&null_value_handle_);

// Address equality for underlying napi handles.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
export struct virtual_address_equal {
	protected:
		~virtual_address_equal() = default;

	public:
		virtual auto operator()(napi_value left, napi_value right) const -> bool = 0;
};

export struct direct_address_equal final : virtual_address_equal {
		auto operator()(napi_value left, napi_value right) const -> bool final {
			return left == right;
		}
};

export struct indirect_address_equal final : virtual_address_equal {
		auto operator()(napi_value left, napi_value right) const -> bool final {
			auto* indirect_left = reinterpret_cast<void**>(left);
			auto* indirect_right = reinterpret_cast<void**>(right);
			return *indirect_left == *indirect_right;
		}
};

// Address hash for napi value handles.
export struct direct_address_hash : std::hash<void*> {};

export struct indirect_address_hash : std::hash<void*> {
	private:
		using std::hash<void*>::operator();

	public:
		auto operator()(napi_value value) const -> std::size_t {
			auto* indirect_handle = reinterpret_cast<void**>(value);
			return (*this)(*indirect_handle);
		}
};

} // namespace js::napi

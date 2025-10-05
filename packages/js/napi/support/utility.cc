module;
#include <functional>
export module napi_js:utility;
import nodejs;

namespace js::napi {

// See: `v8_enable_direct_handle`.

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
			if (left == nullptr || right == nullptr) {
				return left == right;
			} else {
				auto* indirect_left = std::bit_cast<void**>(left);
				auto* indirect_right = std::bit_cast<void**>(right);
				return *indirect_left == *indirect_right;
			}
		}
};

// Address hash for napi value handles.
export struct direct_address_hash : std::hash<void*> {};

export struct indirect_address_hash : std::hash<void*> {
	private:
		using std::hash<void*>::operator();

	public:
		// `value` may NOT be null
		auto operator()(napi_value value) const -> std::size_t {
			auto* indirect_handle = std::bit_cast<void**>(value);
			return (*this)(*indirect_handle);
		}
};

} // namespace js::napi

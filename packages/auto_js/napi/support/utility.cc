module;
#include "auto_js/no_unique_address.h"
export module napi_js:utility;
import nodejs;
import std;
import util;

namespace js::napi {

// See: `v8_enable_direct_handle`.

// Null handle for both direct and indirect handles
// Note: 1 -> 0x1'0000'0000
const std::uint64_t null_value_handle_data = 0x7f7f'7f7f'7f7f'7f7f;
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
export auto* const null_value_handle = reinterpret_cast<napi_value>(const_cast<std::uint64_t*>(&null_value_handle_data));

// Unwrap the v8 address of the napi handle. This points into the v8 heap.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct virtual_handle_address {
	protected:
		~virtual_handle_address() = default;

	public:
		virtual auto operator()(napi_value value) const noexcept -> void* = 0;
};

struct direct_handle_address final : virtual_handle_address {
		auto operator()(napi_value value) const noexcept -> void* final {
			return value;
		}
};

struct indirect_handle_address final : virtual_handle_address {
		auto operator()(napi_value value) const noexcept -> void* final {
			return *reinterpret_cast<void**>(value);
		}
};

// Address equality for underlying napi handles.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
struct virtual_address_equal {
	protected:
		~virtual_address_equal() = default;

	public:
		virtual auto operator()(napi_value left, napi_value right) const noexcept -> bool = 0;
};

template <class Address>
struct virtual_address_equal_of final : virtual_address_equal {
	public:
		auto operator()(napi_value left, napi_value right) const noexcept -> bool final {
			return address_(left) == address_(right);
		}

	private:
		NO_UNIQUE_ADDRESS Address address_;
};

export using direct_address_equal = virtual_address_equal_of<direct_handle_address>;
export using indirect_address_equal = virtual_address_equal_of<indirect_handle_address>;

// Polymorphic address equality operator
export class address_equal {
	private:
		using virtual_equal_type = util::covariant_value<virtual_address_equal, direct_address_equal, indirect_address_equal>;

	public:
		explicit address_equal(bool uses_direct_handles) :
				equal_{
					uses_direct_handles
						? virtual_equal_type{direct_address_equal{}}
						: virtual_equal_type{indirect_address_equal{}}
				} {}

		auto operator()(napi_value left, napi_value right) const noexcept -> bool {
			// NOLINTNEXTLINE(bugprone-exception-escape)
			return (*equal_)(left, right);
		}

	private:
		virtual_equal_type equal_;
};

// Address hash for napi value handles.
template <class Address>
struct address_hash_of final {
	public:
		auto operator()(napi_value value) const noexcept -> std::size_t {
			return hash_(address_(value));
		}

	private:
		NO_UNIQUE_ADDRESS Address address_;
		NO_UNIQUE_ADDRESS std::hash<void*> hash_;
};

export using direct_address_hash = address_hash_of<direct_handle_address>;
export using indirect_address_hash = address_hash_of<indirect_handle_address>;

} // namespace js::napi

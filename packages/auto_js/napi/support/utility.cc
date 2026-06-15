export module napi_js:utility;
import :support.host;
import nodejs;
import std;

namespace js::napi {

// Equality check for underlying handle. False negatives on primitives.
struct addressof_equal {
		auto operator()(napi_value left, napi_value right) const noexcept -> bool {
			return handle_addressof(left) == handle_addressof(right);
		}
};

// Address hash for napi value handles.
struct addressof_hash : std::hash<void*> {
		using std::hash<void*>::operator();
		auto operator()(napi_value value) const noexcept -> std::size_t {
			return (*this)(handle_addressof(value));
		}
};

// Unordered map for napi values
template <class Type>
using unordered_value_map = std::unordered_map<napi_value, Type, addressof_hash, addressof_equal>;

} // namespace js::napi

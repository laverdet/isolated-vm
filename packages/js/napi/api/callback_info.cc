module;
#include <array>
#include <span>
#include <typeinfo>
export module napi_js:api.callback_info;
import :api.invoke;
import isolated_js;
import ivm.utility;

namespace js::napi {

// Helper which reads arguments from napi invocation
export struct callback_info : util::non_copyable {
	public:
		// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
		callback_info(napi_env env, napi_callback_info info) {
			napi::invoke0(napi_get_cb_info, env, info, &count_, storage_.data(), &this_, &data_);
			if (count_ > storage_.size()) {
				throw js::range_error{u"Too many arguments provided for a function call"};
			}
		}

		[[nodiscard]] auto arguments() const -> std::span<const napi_value> { return std::span{storage_}.first(count_); }
		[[nodiscard]] auto data() const -> void* { return data_; }
		[[nodiscard]] auto this_arg() const -> napi_value { return this_; }

	private:
		napi_value this_;
		std::array<napi_value, 8> storage_;
		std::size_t count_{storage_.size()};
		void* data_;
};

// Common declaration for `napi_type_tag` by type
// Nb: `std::type_info::hash_code()` is not constexpr
export template <class Type>
const auto type_tag_for = napi_type_tag{.lower = typeid(Type).hash_code(), .upper = 0};

} // namespace js::napi

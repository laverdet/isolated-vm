export module backend_napi_v8:utility;
import :environment;
import auto_js;
import napi_js;
import std;
import util;

namespace backend_napi_v8 {
using namespace js;

// `structuredClone`-style transfer options
struct transfer_options : js::optional_constructible {
		using js::optional_constructible::optional_constructible;
		std::optional<js::forward<js::napi::local_of<js::list_tag>>> transfer;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"transfer">, &transfer_options::transfer},
		};
};

using transfer_list_type = js::napi::transfer_list<js::napi::array_buffer_transfer>;

// Wrapper for results from a function which could throw on user-supplied conditions
template <class Expected>
struct normal_completion_record {
	public:
		explicit normal_completion_record(Expected result) : result_{std::move(result)} {}

		[[nodiscard]] auto value() && -> Expected { return std::move(result_); }

		constexpr static auto variant = js::discriminated_alternative{util::cw<"complete">, util::cw<true>};
		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"result">, util::fn<&normal_completion_record::value>},
		};

	private:
		Expected result_;
};

struct throw_completion_record {
	public:
		explicit throw_completion_record(js::error_value error) : error_{std::move(error)} {}
		[[nodiscard]] auto error() && -> js::error_value { return std::move(error_); }

		constexpr static auto variant = js::discriminated_alternative{util::cw<"complete">, util::cw<false>};
		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"error">, util::fn<&throw_completion_record::error>},
		};

	private:
		js::error_value error_;
};

template <class Expected>
using completion_record = std::expected<normal_completion_record<Expected>, throw_completion_record>;

// Wraps a `std::expected` operation result into a `completion_record`
template <class Expected>
auto make_completion_record(std::expected<Expected, js::error_value> result) -> completion_record<Expected> {
	return completion_record<Expected>{std::move(result)};
}

} // namespace backend_napi_v8

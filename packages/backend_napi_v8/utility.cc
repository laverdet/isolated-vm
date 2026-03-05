module;
#include <expected>
#include <utility>
export module backend_napi_v8:utility;
import :environment;
import auto_js;
import napi_js;
import util;

namespace backend_napi_v8 {
using namespace js;

export template <class Type>
struct normal_completion_record {
	public:
		explicit normal_completion_record(Type result) : result_{std::move(result)} {}

		[[nodiscard]] auto value() && -> Type { return std::move(result_); }

		constexpr static auto struct_template = js::struct_template{
			js::struct_constant{util::cw<"complete">, true},
			js::struct_accessor{util::cw<"result">, util::fn<&normal_completion_record::value>},
		};

	private:
		Type result_;
};

export struct throw_completion_record {
	public:
		explicit throw_completion_record(js::error_value error) : error_{std::move(error)} {}
		[[nodiscard]] auto error() && -> js::error_value { return std::move(error_); }

		constexpr static auto struct_template = js::struct_template{
			js::struct_constant{util::cw<"complete">, false},
			js::struct_accessor{util::cw<"error">, util::fn<&throw_completion_record::error>},
		};

	private:
		js::error_value error_;
};

export template <class Expect>
auto make_completion_record(environment& env, std::expected<Expect, js::error_value> result) {
	if (result) {
		return js::forward{js::transfer_in_strict<napi_value>(normal_completion_record{*std::move(result)}, env)};
	} else {
		return js::forward{js::transfer_in_strict<napi_value>(throw_completion_record{std::move(result).error()}, env)};
	}
}

} // namespace backend_napi_v8

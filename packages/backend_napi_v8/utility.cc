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

// Wrap a function to return `js::forward<Result>`
// TODO: Move it to `js`
export template <class Function>
auto make_forward_callback(Function function) {
	constexpr auto make =
		[]<class... Args, bool Nx, class Result>(
			std::type_identity<auto(Args...) noexcept(Nx)->Result> /*signature*/,
			auto function
		) -> auto {
		using function_type = decltype(function);
		return util::bind{
			[](function_type& function, Args... args) noexcept(Nx) -> js::forward<Result> {
				return js::forward{function(std::forward<Args>(args)...)};
			},
			std::move(function),
		};
	};
	using signature = util::function_signature_t<Function>;
	return make(std::type_identity<signature>{}, std::move(function));
}

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
		return js::forward{js::transfer_in<napi_value>(normal_completion_record{*std::move(result)}, env)};
	} else {
		return js::forward{js::transfer_in<napi_value>(throw_completion_record{std::move(result).error()}, env)};
	}
}

} // namespace backend_napi_v8

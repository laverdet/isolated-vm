export module backend_napi_v8:utility;
import :environment;
import auto_js;
import std;
import util;

namespace backend_napi_v8 {
using namespace js;

template <class Expected>
struct normal_completion_record {
	public:
		explicit normal_completion_record(Expected result) : result_{std::move(result)} {}

		[[nodiscard]] auto value() && -> Expected { return std::move(result_); }

		constexpr static auto struct_template = js::struct_template{
			js::struct_constant{util::cw<"complete">, true},
			js::struct_accessor{util::cw<"result">, util::fn<&normal_completion_record::value>},
		};

	private:
		Expected result_;
};

struct throw_completion_record {
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

template <class Expected>
struct completion_record : std::expected<normal_completion_record<Expected>, throw_completion_record> {
		using std::expected<normal_completion_record<Expected>, throw_completion_record>::expected;
};

template <class Expected>
completion_record(std::expected<Expected, js::error_value>) -> completion_record<Expected>;

} // namespace backend_napi_v8

namespace js {
using namespace backend_napi_v8;

// TODO: Allow `bool` to be a union discriminant, `std::expected` to be a variant, and unions to be
// visitable. In place of those features, a quick custom visitor is used.
template <class Meta, class Expected>
struct visit<Meta, completion_record<Expected>>
		: visit<Meta, normal_completion_record<Expected>>,
			visit<Meta, throw_completion_record> {
	private:
		using visit_expected_type = visit<Meta, normal_completion_record<Expected>>;
		using visit_throw_type = visit<Meta, throw_completion_record>;

	public:
		explicit constexpr visit(auto* transfer) :
				visit_expected_type{transfer},
				visit_throw_type{transfer} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject) {
				return visit_expected_type::operator()(*std::forward<decltype(subject)>(subject), accept);
			} else {
				return visit_throw_type::operator()(std::forward<decltype(subject)>(subject).error(), accept);
			}
		}

		consteval static auto types(auto recursive) {
			return visit_expected_type::types(recursive) + visit_throw_type::types(recursive);
		}
};

} // namespace js

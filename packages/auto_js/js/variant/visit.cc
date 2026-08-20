export module auto_js:variant.visit;
import :transfer;
import :variant.types;
import std;
import util;

namespace js {

// `std::expected` visits as a variant of its value & error types.
template <class Meta, class Value, class Error>
struct visit<Meta, std::expected<Value, Error>>
		: visit<Meta, Value>,
			visit<Meta, Error> {
	private:
		using visit_value_type = visit<Meta, Value>;
		using visit_error_type = visit<Meta, Error>;

	public:
		constexpr explicit visit(auto* transfer) :
				visit_value_type{transfer},
				visit_error_type{transfer} {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			if (subject) {
				return util::invoke_as<visit_value_type>(*this, *std::forward<decltype(subject)>(subject), accept);
			} else {
				return util::invoke_as<visit_error_type>(*this, std::forward<decltype(subject)>(subject).error(), accept);
			}
		}

		consteval static auto types(auto recursive) -> auto {
			return visit_value_type::types(recursive) + visit_error_type::types(recursive);
		}
};

// `std::variant` visitor.
template <class Meta, class... Types>
struct visit<Meta, std::variant<Types...>> : visit<Meta, Types>... {
		constexpr explicit visit(auto* transfer) :
				visit<Meta, Types>{transfer}... {}

		template <class Accept>
		constexpr auto operator()(auto&& subject, const Accept& accept) -> accept_target_t<Accept> {
			using target_type = accept_target_t<Accept>;
			const auto visit_alternative =
				[ & ](auto index) constexpr -> target_type {
				using visit_type = visit<Meta, Types...[ index ]>;
				return util::invoke_as<visit_type>(*this, std::get<index>(std::forward<decltype(subject)>(subject)), accept);
			};
			return util::template_switch(
				subject.index(),
				util::sequence_cw<sizeof...(Types)>,
				util::overloaded{
					visit_alternative,
					[] -> target_type { std::unreachable(); },
				}
			);
		}

		consteval static auto types(auto recursive) -> auto {
			return (util::type_pack{} + ... + visit<Meta, Types>::types(recursive));
		}
};

} // namespace js

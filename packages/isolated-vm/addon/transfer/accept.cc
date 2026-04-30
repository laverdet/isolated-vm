module;
#include "auto_js/export_tag.h"
export module isolated_vm:transfer.accept;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

struct EXPORT accept_vm_primitive {
	public:
		explicit accept_vm_primitive(environment& env) : env_{env} {}

		// undefined
		auto operator()(undefined_tag tag, visit_holder visit, std::monostate subject) const -> value_of<undefined_tag>;
		auto operator()(undefined_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<undefined_tag> {
			return (*this)(tag, visit, std::monostate{});
		}

		// null
		auto operator()(null_tag tag, visit_holder visit, std::nullptr_t subject) const -> value_of<null_tag>;
		auto operator()(null_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<null_tag> {
			return (*this)(tag, visit, nullptr);
		}

		// boolean
		auto operator()(boolean_tag tag, visit_holder visit, bool subject) const -> value_of<boolean_tag>;

		// number
		auto operator()(number_tag_of<double> tag, visit_holder visit, double subject) const -> value_of<number_tag_of<double>>;
		auto operator()(number_tag_of<std::int32_t> tag, visit_holder visit, std::int32_t) const -> value_of<number_tag_of<std::int32_t>>;
		auto operator()(number_tag_of<std::uint32_t> tag, visit_holder visit, std::uint32_t) const -> value_of<number_tag_of<std::uint32_t>>;

		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<double>> {
			return (*this)(number_tag_of<double>{}, visit, double{std::forward<decltype(subject)>(subject)});
		}

		template <class Type>
		auto operator()(number_tag_of<Type> tag, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<Type>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// string
		auto operator()(string_tag_of<char> tag, visit_holder visit, std::string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char>>>;
		auto operator()(string_tag_of<char16_t> tag, visit_holder visit, std::u16string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char16_t>>>;
		auto operator()(string_tag_of<char8_t> tag, visit_holder visit, std::u8string_view subject) const
			-> js::referenceable_value<value_of<string_tag>>;

		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag>> {
			auto value = (*this)(string_tag_of<char16_t>{}, visit, std::u16string_view{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<string_tag>{*value}};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)});
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string<Char>{std::forward<decltype(subject)>(subject)});
		}

		// bigint (why does `NewFromWords` need a context?)
		auto operator()(bigint_tag_of<std::int64_t> tag, visit_holder visit, std::int64_t subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<std::int64_t>>>;
		auto operator()(bigint_tag_of<std::uint64_t> tag, visit_holder visit, std::uint64_t subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<std::uint64_t>>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, const js::bigint& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>>;

		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag>> {
			auto value = (*this)(bigint_tag_of<js::bigint>{}, visit, js::bigint{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<bigint_tag>{*value}};
		}

		template <class Type>
		auto operator()(bigint_tag_of<Type> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<Type>>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		std::reference_wrapper<environment> env_;
};

} // namespace isolated_vm

namespace js {
using namespace isolated_vm;

template <class Tag>
struct accept<void, value_of<Tag>> : accept_vm_primitive {
		using accept_vm_primitive::accept_vm_primitive;
};

} // namespace js

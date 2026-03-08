module;
#include <string> // IWYU pragma: keep
#include <type_traits>
export module napi_js:string_table;
import :reference;
import auto_js;
import util;
using namespace std::string_literals;

namespace js::napi {

// Given a pack of strings it returns a static reference to a `util::sealed_map` of napi string
// references.
template <const auto& Strings>
constexpr auto string_table_of = []() -> auto {
	const auto [... strings ] = Strings;
	return util::sealed_map{std::type_identity<napi::reference<string_tag>>{}, strings...};
}();

// You can turn off `strict` if you don't care about a string table
struct string_table_options {
		bool strict = true;
};

// Environment storage for string literals
export template <const auto& Strings, string_table_options Options = {}>
class string_table {
	public:
		auto string_table_storage(const auto& string_value) {
			constexpr auto string_sv = util::make_string_view(string_value);
			constexpr auto index = string_table_of<Strings>.lookup(string_sv);
			if constexpr (index) {
				return util::just<napi::reference<string_tag>&>{string_literal_storage_.at(index).second};
			} else {
				// static_assert(!Options.strict, std::format("String literal '{}' is missing in storage", string_sv));
				static_assert(!Options.strict, "String literal '"s + string_sv + "' is missing in storage"s);
				return util::nothing<napi::reference<string_tag>&>{};
			}
		}

	private:
		util::copy_of<string_table_of<Strings>> string_literal_storage_;
};

} // namespace js::napi

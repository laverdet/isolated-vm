module;
#include <string>
#include <type_traits>
export module napi_js:class_template_storage;
import :reference;
import auto_js;
import util;
using namespace std::string_literals;

namespace js::napi {

// Given a pack of strings it returns a static reference to a `util::sealed_map` of napi object
// references.
template <const auto& Strings>
constexpr auto class_template_references_of = []() -> auto {
	const auto [... strings ] = Strings;
	return util::sealed_map{std::type_identity<napi::reference<class_tag>>{}, strings...};
}();

// Environment storage for class template references
export template <const auto& Strings>
class class_template_references {
	public:
		template <class Type>
		auto class_template(this auto& self, std::type_identity<Type> /*type*/, const auto& class_template) -> napi::value<class_tag_of<Type>> {
			constexpr auto name_sv = util::make_string_view(class_template.constructor.name);
			constexpr auto index = class_template_references_of<Strings>.lookup(name_sv);
			if constexpr (!index) {
				// nb: This fails while linking against STL for some reason. So the `constexpr` if works
				// around the issue.
				// D:\a\isolated-vm\isolated-vm\packages\auto_js\napi\support\class_template_storage.cc:38:45:
				//    error: invalid operands to binary expression ('string' (aka 'basic_string<char,
				//    char_traits<char>, allocator<char>>') and 'const std::basic_string_view<char>')
				//    38 |                         static_assert(index, "Class template '"s + name_sv + "' is missing in storage"s);
				static_assert(index, "Class template '"s + name_sv + "' is missing in storage"s);
			}
			auto& reference = self.class_template_references_.at(index).second;
			using value_type = js::napi::value<class_tag_of<Type>>;
			if (reference) {
				return value_type::from(reference.get(self));
			} else {
				auto template_value = value_type::make(self, class_template);
				reference.reset(self, template_value);
				return template_value;
			}
		}

	private:
		util::copy_of<class_template_references_of<Strings>> class_template_references_;
};

} // namespace js::napi

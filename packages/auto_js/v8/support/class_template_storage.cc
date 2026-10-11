export module v8_js:class_template_storage;
import :class_;
import auto_js;
import std;
import util;
import v8;
using namespace std::string_literals;

namespace js::iv8 {

// Given a pack of strings it returns a static reference to a `util::sealed_map` of class template
// indices.
template <const auto& Strings>
constexpr auto class_template_indices_of = [] consteval -> auto {
	const auto [... strings ] = Strings;
	return util::sealed_map{std::type_identity<std::monostate>{}, strings...};
}();

// Environment storage for class templates
export template <const auto& Strings>
class class_template_references {
	public:
		template <class Type>
		auto class_template(std::type_identity<Type> /*type*/, const auto& lock, const auto& class_template) -> v8::Local<class_template_of<Type>> {
			constexpr auto name_sv = util::make_consteval_string_view(class_template.constructor.name);
			constexpr auto index = class_template_indices_of<Strings>.lookup(name_sv);
			static_assert(index, "Class template is missing in storage");
			auto& reference = references_.at(*index);
			if (reference.IsEmpty()) {
				auto template_value = class_template_of<Type>::make(lock, class_template);
				reference.Reset(lock.isolate(), template_value);
				return template_value;
			} else {
				return reference.Get(lock.isolate()).template As<class_template_of<Type>>();
			}
		}

	private:
		std::array<v8::Global<v8::FunctionTemplate>, std::tuple_size_v<std::remove_cvref_t<decltype(Strings)>>> references_;
};

} // namespace js::iv8

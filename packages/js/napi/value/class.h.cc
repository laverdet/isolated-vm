module;
#include <concepts>
export module napi_js:class_;
import :value_handle;
import :object;
import ivm.utility;

namespace js::napi {

template <class Type>
class value<class_tag_of<Type>> : public value_next<class_tag_of<Type>> {
	public:
		using value_next<class_tag_of<Type>>::value_next;

		// Construct a new C++ instance & JavaScript value
		auto construct(auto& env, auto&&... args) const -> value<object_tag>
			requires std::constructible_from<Type, decltype(args)...>;

		// Create a JavaScript value from an already-created instance smart pointer
		auto transfer(auto& env, auto instance) const -> value<object_tag>;

		template <class Environment>
		static auto make(Environment& env, const auto& class_template) -> value<class_tag_of<Type>>;
};

} // namespace js::napi

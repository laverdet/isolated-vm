module;
#include <concepts>
#include <tuple>
export module napi_js:class_;
import :value_handle;
import :object;

namespace js::napi {

template <class Type>
class value<class_tag_of<Type>> : public value_next<class_tag_of<Type>> {
	public:
		using value_next<class_tag_of<Type>>::value_next;

		// Construct a new C++ instance & JavaScript value
		template <class... Args>
		auto construct(auto& env, Args&&... args) const -> value<object_tag>
			requires std::constructible_from<Type, Args...>;

		template <class... HostArgs, class... RuntimeArgs>
		auto runtime_construct(auto& env, std::tuple<HostArgs...> host_args, std::tuple<RuntimeArgs...> runtime_args) const -> value<object_tag>
			requires std::constructible_from<Type, HostArgs...>;

		// Create a JavaScript value from an already-created instance smart pointer
		template <class... Args>
		auto transfer_construct(auto& env, auto instance, std::tuple<Args...> runtime_args) const -> value<object_tag>;

		template <class Environment>
		static auto make(Environment& env, const auto& class_template) -> value<class_tag_of<Type>>;
};

} // namespace js::napi

export module napi_js:class_;
import :lock;
import :object;
import std;

namespace js::napi {

template <class Type>
class local_for_class_of : public local_next<class_tag_of<Type>> {
	public:
		// Construct a new C++ instance & JavaScript value
		template <class... Args>
		auto construct(const auto& lock, Args&&... args) const -> local_of<object_tag>
			requires std::constructible_from<Type, Args...>;

		template <class... HostArgs, class... RuntimeArgs>
		auto runtime_construct(const auto& lock, std::tuple<HostArgs...> host_args, std::tuple<RuntimeArgs...> runtime_args) const -> local_of<object_tag>
			requires std::constructible_from<Type, HostArgs...>;

		// Create a JavaScript value from an already-created instance smart pointer
		template <class... Args>
		auto transfer_construct(const auto& lock, auto instance, std::tuple<Args...> runtime_args) const -> local_of<object_tag>;

		template <class Environment>
		static auto make(const environment_lock_witness_of<Environment>& lock, const auto& class_template) -> local_of<class_tag_of<Type>>;
};

} // namespace js::napi

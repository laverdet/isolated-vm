export module v8_js:class_;
import std;
import v8;

namespace js::iv8 {

// `v8::FunctionTemplate` of a host class. Instances are wrapped with a cppgc-managed `Type`, which
// `value_for_object::try_cast` recovers.
export template <class Type>
class class_template_of : public v8::FunctionTemplate {
	public:
		// Construct a new C++ instance & JavaScript value
		template <class... Args>
		auto construct(const auto& lock, Args&&... args) -> v8::Local<v8::Object>
			requires std::constructible_from<Type, Args...>;

		// Instantiate the class template from a `js::class_template` definition
		static auto make(const auto& lock, const auto& class_template) -> v8::Local<class_template_of>;
};

} // namespace js::iv8

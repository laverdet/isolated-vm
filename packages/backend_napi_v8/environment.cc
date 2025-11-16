module;
#include <string_view>
#include <type_traits>
export module backend_napi_v8:environment;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
using namespace js;
using namespace std::string_view_literals;

namespace backend_napi_v8 {

// Storage for string literals used in this module
constexpr auto string_literals = util::sealed_map{
	std::type_identity<napi::reference<string_tag>>{},
	"attributes"sv,
	"clock"sv,
	"column"sv,
	"complete"sv,
	"epoch"sv,
	"error"sv,
	"interval"sv,
	"line"sv,
	"location"sv,
	"modules"sv,
	"name"sv,
	"origin"sv,
	"payload"sv,
	"randomSeed"sv,
	"result"sv,
	"specifier"sv,
	"timeout"sv,
	"type"sv,
};

// Storage for class templates
constexpr auto class_templates = util::sealed_map{
	std::type_identity<napi::reference<class_tag>>{},
	u8"Agent"sv,
	u8"Module"sv,
	u8"Realm"sv,
	u8"Script"sv,
	u8"SubscriberCapability"sv,
};

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment : public napi::environment_of<environment> {
	public:
		explicit environment(napi_env env) : environment_of{env} {}

		auto cluster() -> isolated_v8::cluster& { return cluster_; }

		// Lookup `reference<T>` for the given class template
		template <class Type>
		auto class_template(std::type_identity<Type> /*type*/, const auto& class_template) {
			constexpr auto index = class_templates.lookup(std::u8string_view{class_template.constructor.name});
			static_assert(index, "Class template is missing in storage");
			auto reference = class_template_storage_.at(index).second;
			using value_type = js::napi::value<class_tag_of<Type>>;
			if (reference) {
				return value_type::from(reference.get(*this));
			} else {
				auto template_value = value_type::make(*this, class_template);
				reference.reset(*this, template_value);
				return template_value;
			}
		}

		// Lookup `reference<T>` for the given literal
		auto global_storage(const auto& string_value) -> auto& {
			constexpr auto index = string_literals.lookup(std::string_view{string_value});
			static_assert(index, "String literal is missing in storage");
			// static_assert(index, std::format("String literal '{}' is missing in storage", Value.data()));
			return string_literal_storage_.at(index).second;
		}

	private:
		isolated_v8::cluster cluster_;
		util::copy_of<class_templates> class_template_storage_;
		util::copy_of<string_literals> string_literal_storage_;
};

} // namespace backend_napi_v8

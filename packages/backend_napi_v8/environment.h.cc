export module backend_napi_v8:environment;
import auto_js;
import napi_js;
import std;
import v8_js;
using namespace js;
using namespace std::string_view_literals;

namespace backend_napi_v8 {

// String literals used in this module
constexpr auto string_literals = std::tuple{
	"Agent"sv,
	"attributes"sv,
	"clock"sv,
	"column"sv,
	"complete"sv,
	"copy"sv,
	"epoch"sv,
	"error"sv,
	"get"sv,
	"initialize"sv,
	"interval"sv,
	"line"sv,
	"location"sv,
	"Module"sv,
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
constexpr auto class_names = std::tuple{
	"Agent"sv,
	"Module"sv,
	"Realm"sv,
	"Reference"sv,
	"Script"sv,
	"SubscriberCapability"sv,
};

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment
		: public napi::environment,
			public napi::string_table<string_literals>,
			public napi::class_template_references<class_names> {
	public:
		using napi::environment::environment;

		auto agent_class() -> napi::value<function_tag> { return agent_class_.get(*this); }
		auto cluster() -> js::iv8::isolated::cluster& { return cluster_; }
		auto module_class() -> napi::value<function_tag> { return module_class_.get(*this); }

		auto make_initialize() -> napi::value<function_tag>;

	private:
		iv8::isolated::cluster cluster_;
		napi::reference<function_tag> agent_class_;
		napi::reference<function_tag> module_class_;
};

} // namespace backend_napi_v8

module;
#include <ranges>
#include <utility>
#include <vector>
module ivm.isolated_v8;
import :agent;
import :js_module;
import :realm;
import ivm.iv8;
import ivm.value;
import v8;

namespace ivm {

// module_request
module_request::module_request(value::string_t specifier, attributes_type attributes_) :
		specifier{std::move(specifier)},
		attributes_{std::move(attributes_)} {
}

// js_module
js_module::js_module(agent::lock& agent, v8::Local<v8::Module> module_) :
		module_{agent->isolate(), module_} {
}

auto js_module::requests(realm::scope& realm) -> std::vector<module_request> {
	auto requests_array = iv8::fixed_array{module_.Get(realm.isolate())->GetModuleRequests(), realm.context()};
	auto requests_view =
		requests_array |
		std::views::transform([ & ](v8::Local<v8::Data> value) -> module_request {
			auto request = value.As<v8::ModuleRequest>();
			auto visit_args = std::tuple{realm.isolate(), realm.context()};
			auto specifier_handle = request->GetSpecifier().As<v8::String>();
			auto specifier = value::transfer_strict<value::string_t>(specifier_handle, visit_args, std::tuple{});
			auto attributes_view =
				// [ key, value, location, ...[] ]
				iv8::fixed_array{request->GetImportAttributes(), realm.context()} |
				std::views::chunk(3) |
				std::views::transform([ & ](const auto& triplet) {
					return std::pair{
						value::transfer_strict<value::string_t>(triplet[ 0 ].template As<v8::String>(), visit_args, std::tuple{}),
						value::transfer_strict<value::string_t>(triplet[ 1 ].template As<v8::String>(), visit_args, std::tuple{})
					};
				});
			return {specifier, module_request::attributes_type{attributes_view}};
		});
	return {requests_view.begin(), requests_view.end()};
}

auto js_module::compile(realm::scope& realm, v8::Local<v8::String> source_text, const source_origin& source_origin) -> js_module {
	v8::ScriptOrigin origin{
		source_origin.resource_name(),
		source_origin.location().line(),
		source_origin.location().column(),
		false,
		-1,
		v8::Local<v8::Value>{},
		false,
		false,
		true,
	};
	v8::ScriptCompiler::Source source{source_text, origin};
	auto script_handle = v8::ScriptCompiler::CompileModule(realm.isolate(), &source).ToLocalChecked();
	return js_module{realm.agent(), script_handle};
}

} // namespace ivm

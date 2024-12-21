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
module_request::module_request(value::string_t specifier, attributes_type attributes) :
		attributes_{std::move(attributes)},
		specifier_{std::move(specifier)} {}

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
						value::transfer<value::string_t>(triplet[ 0 ].template As<v8::Name>(), visit_args, std::tuple{}),
						value::transfer<value::string_t>(triplet[ 1 ].template As<v8::Value>(), visit_args, std::tuple{})
					};
				});
			return {specifier, module_request::attributes_type{attributes_view}};
		});
	return {requests_view.begin(), requests_view.end()};
}

auto js_module::compile(realm::scope& realm, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module {
	auto maybe_resource_name = value::transfer_strict<v8::MaybeLocal<v8::String>>(
		source_origin.name,
		std::tuple{},
		std::tuple{realm.isolate()}
	);
	v8::Local<v8::String> resource_name{};
	(void)maybe_resource_name.ToLocal(&resource_name);
	auto location = source_origin.location.value_or(source_location{});
	v8::ScriptOrigin origin{
		resource_name,
		location.line,
		location.column,
		false,
		-1,
		v8::Local<v8::Value>{},
		false,
		false,
		true,
	};
	v8::ScriptCompiler::Source source{source_text, origin};
	auto module_handle = v8::ScriptCompiler::CompileModule(realm.isolate(), &source).ToLocalChecked();
	v8::Global<v8::Module> module_global{realm.isolate(), module_handle};
	if (source_origin.name) {
		realm.agent()->weak_module_specifiers().insert(
			realm.isolate(),
			std::pair{module_handle, *std::move(source_origin.name)}
		);
	}
	return js_module{realm.agent(), module_handle};
}

} // namespace ivm

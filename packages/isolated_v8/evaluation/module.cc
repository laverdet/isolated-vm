module;
#include <cassert>
#include <ranges>
#include <utility>
#include <vector>
module isolated_v8;
import :agent_host;
import :evaluation_module_action;
import :realm;
import :remote;
import v8_js;
import isolated_js;
import v8;

namespace isolated_v8 {

// module_request
module_request::module_request(js::string_t specifier, attributes_type attributes) :
		attributes_{std::move(attributes)},
		specifier_{std::move(specifier)} {}

// js_module
js_module::js_module(const agent_lock& agent, v8::Local<v8::Module> module_) :
		module_{make_shared_remote(agent, module_)} {
}

auto js_module::requests(const agent_lock& agent) -> std::vector<module_request> {
	auto context = agent->scratch_context();
	auto requests_array = js::iv8::fixed_array{context, module_->deref(agent)->GetModuleRequests()};
	auto requests_view =
		requests_array |
		std::views::transform([ & ](v8::Local<v8::Data> value) -> module_request {
			auto request = value.As<v8::ModuleRequest>();
			auto specifier_handle = request->GetSpecifier().As<v8::String>();
			auto specifier = js::transfer_out_strict<js::string_t>(specifier_handle, agent);
			auto attributes_view =
				// [ key, value, location, ...[] ]
				js::iv8::fixed_array{context, request->GetImportAttributes()} |
				std::views::chunk(3) |
				std::views::transform([ & ](const auto& triplet) {
					auto entry = js::transfer_out_strict<std::array<js::string_t, 2>>(
						std::array{
							triplet[ 0 ].template As<v8::String>(),
							triplet[ 1 ].template As<v8::String>()
						},
						agent
					);
					return std::pair{std::move(entry[ 0 ]), std::move(entry[ 1 ])};
				});
			return {specifier, module_request::attributes_type{std::from_range, std::move(attributes_view)}};
		});
	return {std::from_range, std::move(requests_view)};
}

auto js_module::evaluate(const realm::scope& realm) -> js::value_t {
	auto module_handle = module_->deref(realm);
	auto result = module_handle->Evaluate(realm.context()).ToLocalChecked();
	auto promise = result.As<v8::Promise>();
	if (module_handle->IsGraphAsync()) {
		throw std::runtime_error{"Module is async"};
	} else {
		auto resolved_result = promise->Result();
		return js::transfer_out<js::value_t>(resolved_result, realm);
	}
}

auto js_module::create_synthetic(
	const agent_lock& agent,
	std::span<const v8::Local<v8::String>> export_names,
	source_required_name source_origin,
	synthetic_module_action_type action
) -> js_module {
	v8::Module::SyntheticModuleEvaluationSteps evaluation_steps =
		[](v8::Local<v8::Context> context, v8::Local<v8::Module> module) -> v8::MaybeLocal<v8::Value> {
		auto& host = *agent_host::get_current();
		auto implicit_witness = js::iv8::isolate_lock_witness::make_witness(host.isolate());
		host.weak_module_actions().extract(implicit_witness, module)(context, module);
		auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
		resolver->Resolve(context, v8::Undefined(host.isolate())).ToChecked();
		return resolver->GetPromise();
	};
	auto* const isolate = agent->isolate();
	const auto v8_export_names = v8::MemorySpan<const v8::Local<v8::String>>{export_names.begin(), export_names.end()};
	const auto module_name = js::transfer_in_strict<v8::Local<v8::String>>(std::move(source_origin).name, agent);
	const auto module_handle = v8::Module::CreateSyntheticModule(isolate, module_name, v8_export_names, evaluation_steps);
	[[maybe_unused]] const auto insertion_result =
		agent->weak_module_actions().emplace(agent, std::pair{module_handle, std::move(action)});
	assert(insertion_result);
	return js_module{agent, module_handle};
}

auto js_module::compile(const agent_lock& agent, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module {
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(source_origin.name, agent);
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
	auto module_handle = v8::ScriptCompiler::CompileModule(agent->isolate(), &source).ToLocalChecked();
	v8::Global<v8::Module> module_global{agent->isolate(), module_handle};
	if (source_origin.name) {
		agent->weak_module_specifiers().emplace(agent, std::pair{module_handle, *std::move(source_origin.name)});
	}
	return js_module{agent, module_handle};
}

auto js_module::link(const realm::scope& realm, v8::Module::ResolveModuleCallback callback) -> void {
	auto module = module_->deref(realm);
	module->InstantiateModule(realm.context(), callback).ToChecked();
}

} // namespace isolated_v8

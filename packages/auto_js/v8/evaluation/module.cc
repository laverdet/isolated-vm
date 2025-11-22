module;
#include <cassert>
#include <exception>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
module v8_js;
import :evaluation.module_record;
import :fixed_array;
import auto_js;
import v8;

namespace js::iv8 {

auto module_record::requests(context_lock_witness lock, v8::Local<v8::Module> module) -> std::vector<module_request> {
	auto requests_array = fixed_array{lock.context(), module->GetModuleRequests()};
	auto requests_view =
		requests_array |
		std::views::transform([ & ](v8::Local<v8::Data> value) -> module_request {
			auto request = value.As<v8::ModuleRequest>();
			auto specifier_handle = request->GetSpecifier().As<v8::String>();
			auto specifier = js::transfer_out_strict<std::u16string>(specifier_handle, lock);
			auto attributes_view =
				// [ key, value, location, ...[] ]
				fixed_array{lock.context(), request->GetImportAttributes()} |
				std::views::chunk(3) |
				std::views::transform([ & ](const auto& triplet) {
					auto entry = js::transfer_out_strict<std::array<std::u16string, 2>>(
						std::array{
							triplet[ 0 ].template As<v8::String>(),
							triplet[ 1 ].template As<v8::String>()
						},
						lock
					);
					return std::pair{std::move(entry[ 0 ]), std::move(entry[ 1 ])};
				});
			return {specifier, module_request::attributes_type{std::from_range, attributes_view}};
		});
	return {std::from_range, std::move(requests_view)};
}

auto module_record::compile(context_lock_witness lock, v8::Local<v8::String> source_text, iv8::source_origin origin) -> expected_module_type {
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(origin.name, lock);
	v8::Local<v8::String> resource_name{};
	// nb: Empty handle is ok for `v8::ScriptOrigin`
	(void)maybe_resource_name.ToLocal(&resource_name);
	auto location = origin.location.value_or(source_location{});
	const auto script_origin = v8::ScriptOrigin{
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
	v8::ScriptCompiler::Source source{source_text, script_origin};
	auto maybe_module = unmaybe_one(lock, [ & ] -> v8::MaybeLocal<v8::Module> {
		return v8::ScriptCompiler::CompileModule(lock.isolate(), &source);
	});
	return maybe_module;
}

auto module_record::create_synthetic(context_lock_witness lock, string_span export_names, synthetic_module_action_type action, v8::Local<v8::String> module_name) -> v8::Local<v8::Module> {
	thread_local v8::Isolate* tl_isolate;
	thread_local synthetic_module_action_type* tl_synthetic_action;
	tl_isolate = lock.isolate();
	tl_synthetic_action = &action;

	// Make `v8::Module` record with immediate evaluation steps
	v8::Module::SyntheticModuleEvaluationSteps evaluation_steps =
		[](v8::Local<v8::Context> context, v8::Local<v8::Module> module) -> v8::MaybeLocal<v8::Value> {
		auto& action = *tl_synthetic_action;
		auto isolate_witness = isolate_lock_witness::make_witness(tl_isolate);
		auto context_witness = context_lock_witness::make_witness(isolate_witness, context);
		action(context_witness, module);
		auto resolver = unmaybe(v8::Promise::Resolver::New(context_witness.context()));
		unmaybe(resolver->Resolve(context, v8::Undefined(context_witness.isolate())));
		return resolver->GetPromise();
	};
	auto v8_export_names = v8::MemorySpan<const v8::Local<v8::String>>{export_names.begin(), export_names.end()};
	auto module_record = v8::Module::CreateSyntheticModule(lock.isolate(), module_name, v8_export_names, evaluation_steps);

	// "Link" the module, which is a no-op
	// auto null_callback = v8::Module::ResolveModuleByIndexCallback{nullptr};
	auto null_callback = v8::Module::ResolveModuleCallback{nullptr};
	unmaybe(module_record->InstantiateModule(lock.context(), null_callback));

	// `Evaluate` invokes `evaluation_steps` above
	unmaybe(module_record->Evaluate(lock.context()));
	return module_record;
}

auto module_record::link(context_lock_witness lock, v8::Local<v8::Module> module, module_link_record link_record) -> void {
	// Initialize link state
	auto module_specifier_map = std::unordered_map<v8::Local<v8::Module>, unsigned, address_hash>{};
	auto module_id = 0U;
	for (auto ii = 0UZ; ii < link_record.payload.size();) {
		auto count = link_record.payload.at(ii);
		module_specifier_map.emplace(link_record.modules.at(module_id), static_cast<unsigned>(ii) + 1);
		ii += count + 1;
		++module_id;
	}

	// Linker implementation. We simply assume that module link requests for the same module go in the
	// same order as `GetModuleRequests()`
	auto linker = [ & ](v8::Local<v8::Module> referrer) -> v8::Local<v8::Module> {
		auto it = module_specifier_map.find(referrer);
		if (it == module_specifier_map.end()) {
			throw js::runtime_error{u"Referrer module not found in link record"};
		}
		return link_record.modules.at(link_record.payload.at(it->second++));
	};
	thread_local auto* linker_ptr = &linker;

	// v8 linker callback
	v8::Module::ResolveModuleCallback v8_callback =
		[](
			v8::Local<v8::Context> /*context*/,
			v8::Local<v8::String> /*specifier*/,
			v8::Local<v8::FixedArray> /*attributes*/,
			v8::Local<v8::Module> referrer
		) -> v8::MaybeLocal<v8::Module> {
		return (*linker_ptr)(referrer);
	};
	// auto v8_callback = v8::Module::ResolveModuleByIndexCallback{
	// 	[](
	// 		v8::Local<v8::Context> /*context*/,
	// 		size_t /*module_request_index*/,
	// 		v8::Local<v8::Module> referrer
	// 	) -> v8::MaybeLocal<v8::Module> {
	// 		return (*linker_ptr)(referrer);
	// 	}
	// };
	unmaybe(module->InstantiateModule(lock.context(), v8_callback));
}

auto module_record::link(context_lock_witness lock, v8::Local<v8::Module> module) -> void {
	// Probably a synthetic module
	// auto v8_callback = v8::Module::ResolveModuleByIndexCallback{
	// 	[](
	// 		v8::Local<v8::Context> /*context*/,
	// 		size_t /*module_request_index*/,
	// 		v8::Local<v8::Module> /*referrer*/
	// 	) -> v8::MaybeLocal<v8::Module> {
	// 		std::terminate();
	// 	}
	// };
	v8::Module::ResolveModuleCallback v8_callback =
		[](
			v8::Local<v8::Context> /*context*/,
			v8::Local<v8::String> /*specifier*/,
			v8::Local<v8::FixedArray> /*attributes*/,
			v8::Local<v8::Module> /*referrer*/
		) -> v8::MaybeLocal<v8::Module> {
		std::terminate();
	};
	unmaybe(module->InstantiateModule(lock.context(), v8_callback));
}

auto module_record::evaluate(context_lock_witness lock, v8::Local<v8::Module> module) -> void {
	unmaybe(module->Evaluate(lock.context()));
	if (module->IsGraphAsync()) {
		throw std::runtime_error{"Module is async"};
	}
}

} // namespace js::iv8

module;
#include <cassert>
#include <ranges>
#include <string>
#include <utility>
#include <vector>
module v8_js;
import :evaluation.module_record;
import :fixed_array;
import isolated_js;
import v8;

namespace js::iv8 {
namespace {
thread_local v8::Isolate* tl_isolate;
thread_local const module_specifiers_lock* tl_module_lock;
thread_local module_record::synthetic_module_action_type* tl_synthetic_action;
thread_local module_record::module_link_action_type* tl_link_action;
} // namespace

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

auto module_record::compile(const module_specifiers_lock& lock, v8::Local<v8::String> source_text, iv8::source_origin origin) -> expected_module_type {
	auto maybe_resource_name = js::transfer_in_strict<v8::MaybeLocal<v8::String>>(origin.name, lock.witness());
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
	auto maybe_module = unmaybe_one(lock.witness(), [ & ] -> v8::MaybeLocal<v8::Module> {
		return v8::ScriptCompiler::CompileModule(lock.witness().isolate(), &source);
	});
	if (maybe_module && origin.name) {
		lock.weak_module_specifiers().emplace(util::slice{lock.witness()}, std::pair{maybe_module.value(), *std::move(origin).name});
	}
	return maybe_module;
}

auto module_record::create_synthetic(context_lock_witness lock, string_span export_names, synthetic_module_action_type action, v8::Local<v8::String> module_name) -> v8::Local<v8::Module> {
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
	unmaybe(module_record->InstantiateModule(lock.context(), nullptr));

	// `Evaluate` invokes `evaluation_steps` above
	unmaybe(module_record->Evaluate(lock.context()));
	return module_record;
}

auto module_record::link(const module_specifiers_lock& lock, v8::Local<v8::Module> module, module_link_action_type callback) -> void {
	tl_module_lock = &lock;
	tl_link_action = &callback;

	// Linker callback implementation
	v8::Module::ResolveModuleCallback v8_callback =
		[](
			v8::Local<v8::Context> context,
			v8::Local<v8::String> specifier,
			v8::Local<v8::FixedArray> attributes,
			v8::Local<v8::Module> referrer
		) -> v8::MaybeLocal<v8::Module> {
		const auto& lock = *tl_module_lock;
		auto& action = *tl_link_action;
		auto specifier_string = js::transfer_out_strict<std::u16string>(specifier, lock.witness());
		auto referrer_name = [ & ]() -> std::optional<std::u16string> {
			auto& weak_module_specifiers = lock.weak_module_specifiers();
			const auto* referrer_name = weak_module_specifiers.find(referrer);
			if (referrer_name == nullptr) {
				return std::nullopt;
			} else {
				return *referrer_name;
			}
		}();
		auto attributes_view =
			fixed_array{context, attributes} |
			// [ key, value, ...[] ]
			std::views::chunk(2) |
			std::views::transform([ & ](const auto& pair) -> auto {
				auto entry = js::transfer_out_strict<std::array<std::u16string, 2>>(
					std::array{
						pair[ 0 ].template As<v8::String>(),
						pair[ 1 ].template As<v8::String>()
					},
					lock.witness()
				);
				return std::pair{std::move(entry[ 0 ]), std::move(entry[ 1 ])};
			});
		auto attributes_vector = module_request::attributes_type{std::from_range, std::move(attributes_view)};
		const auto unlocker = isolate_unlock{util::slice{lock.witness()}};
		return action(
			util::slice{lock.witness()},
			std::move(referrer_name),
			module_request{std::move(specifier_string), std::move(attributes_vector)}
		);
	};
	unmaybe(module->InstantiateModule(lock.witness().context(), v8_callback));
}

auto module_record::evaluate(context_lock_witness lock, v8::Local<v8::Module> module) -> void {
	unmaybe(module->Evaluate(lock.context()));
	if (module->IsGraphAsync()) {
		throw std::runtime_error{"Module is async"};
	}
}

} // namespace js::iv8

module;
#include "chunk_view.h"
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>
export module isolated_v8.module_;
import isolated_v8.agent;
import isolated_v8.evaluation.module_action;
import isolated_v8.evaluation.origin;
import isolated_v8.function;
import isolated_v8.realm;
import isolated_v8.remote;
import isolated_v8.script;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;
using namespace util::string_literals;

namespace isolated_v8 {

export class module_request {
	public:
		using attributes_type = js::dictionary<js::dictionary_tag, js::string_t, js::string_t>;
		module_request(js::string_t specifier, attributes_type attributes_);
		[[nodiscard]] auto specifier() const -> const js::string_t& { return specifier_; }

	private:
		attributes_type attributes_;
		js::string_t specifier_;
};

export class js_module {
	public:
		js_module() = delete;
		js_module(agent::lock& agent, v8::Local<v8::Module> module_);

		auto evaluate(realm::scope& realm_scope) -> js::value_t;
		auto link(realm::scope& realm, auto callback) -> void;
		auto requests(agent::lock& agent) -> std::vector<module_request>;

		static auto compile(agent::lock& agent, auto&& source_text, source_origin source_origin) -> js_module;
		static auto create_synthetic(agent::lock& agent, auto exports_default, source_required_name source_origin) -> js_module;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module;
		static auto create_synthetic(
			agent::lock& agent,
			// const v8::MemorySpan<const v8::Local<v8::String>>& export_names,
			source_required_name source_origin,
			synthetic_module_action_type action
		) -> js_module;

		auto link(realm::scope& realm, v8::Module::ResolveModuleCallback callback) -> void;

		shared_remote<v8::Module> module_;
};

auto js_module::compile(agent::lock& agent, auto&& source_text, source_origin source_origin) -> js_module {
	// nb: Context lock is needed for compilation errors
	js::iv8::context_managed_lock context_lock{agent, agent->scratch_context()};
	auto local_source_text = js::transfer_in_strict<v8::Local<v8::String>>(std::forward<decltype(source_text)>(source_text), agent);
	return compile(agent, local_source_text, std::move(source_origin));
}

auto js_module::create_synthetic(agent::lock& agent, auto exports_default, source_required_name source_origin) -> js_module {
	synthetic_module_action_type action =
		[ exports_default = std::move(exports_default) ](
			v8::Local<v8::Context> context,
			v8::Local<v8::Module> module
		) mutable {
			auto& agent = agent::lock::get_current();
			realm::witness_scope realm{agent, context};
			auto name = js::transfer_in_strict<v8::Local<v8::String>>("default", agent);
			auto value = js::transfer_strict<v8::Local<v8::Value>>(std::move(exports_default), std::forward_as_tuple(realm), std::forward_as_tuple(realm));
			module->SetSyntheticModuleExport(agent->isolate(), name, value).ToChecked();
		};
	return create_synthetic(agent, std::move(source_origin), std::move(action));
}

auto js_module::link(realm::scope& realm, auto callback) -> void {
	// Thread locals are used to hold contextual data about this link request. `InstantiateModule`
	// does not provide any user data, but since it is also a blocking operation it's a good place to
	// put this.
	static thread_local realm::scope* realm_local;
	realm_local = &realm;

	static thread_local decltype(callback)* thread_callback_local;
	thread_callback_local = &callback;

	v8::Module::ResolveModuleCallback v8_callback =
		[](
			v8::Local<v8::Context> /*context*/,
			v8::Local<v8::String> specifier,
			v8::Local<v8::FixedArray> attributes,
			v8::Local<v8::Module> referrer
		) -> v8::MaybeLocal<v8::Module> {
		auto& realm = *realm_local;
		auto& thread_callback = *thread_callback_local;
		auto specifier_string = js::transfer_out_strict<js::string_t>(specifier, realm);
		auto referrer_name = std::invoke([ & ]() -> std::optional<js::string_t> {
			auto& weak_module_specifiers = realm.agent()->weak_module_specifiers();
			const auto* referrer_name = weak_module_specifiers.find(referrer);
			if (referrer_name == nullptr) {
				return std::nullopt;
			} else {
				return *referrer_name;
			}
		});
		auto attributes_view =
			js::iv8::fixed_array{realm, attributes} |
			// [ key, value, ...[] ]
			std::views::chunk(2) |
			std::views::transform([ & ](const auto& pair) {
				auto entry = js::transfer_out_strict<std::array<js::string_t, 2>>(
					std::array{
						pair[ 0 ].template As<v8::String>(),
						pair[ 1 ].template As<v8::String>()
					},
					realm
				);
				return std::pair{std::move(entry[ 0 ]), std::move(entry[ 1 ])};
			});
		auto attributes_vector = module_request::attributes_type{std::move(attributes_view)};
		auto& result = std::invoke([ & ]() -> decltype(auto) {
			js::iv8::isolate_unlock unlocker{realm};
			auto&& result = thread_callback(
				std::move(specifier_string),
				std::move(referrer_name),
				std::move(attributes_vector)
			);
			return result;
		});
		return result.module_->deref(realm);
	};
	link(realm, v8_callback);
}

} // namespace isolated_v8

namespace js {
using isolated_v8::module_request;

template <>
struct struct_properties<module_request> {
		constexpr static auto properties = std::tuple{
			property{"specifier"_st, struct_accessor{&module_request::specifier}},
		};
};

} // namespace js

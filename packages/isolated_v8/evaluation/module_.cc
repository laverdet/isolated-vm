module;
#include <functional>
#include <optional>
#include <ranges>
#include <span>
#include <tuple>
#include <utility>
#include <vector>
export module isolated_v8:module_;
import :agent_host;
import :evaluation_module_action;
import :evaluation_origin;
import :realm;
import :script;
import v8_js;
import isolated_js;
import ivm.utility;
import v8;

namespace isolated_v8 {

export class module_request {
	public:
		using attributes_type = js::dictionary<js::dictionary_tag, js::string_t, js::string_t>;
		module_request(js::string_t specifier, attributes_type attributes_);
		[[nodiscard]] auto specifier() const -> const js::string_t& { return specifier_; }

		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"specifier">, util::fn<&module_request::specifier>},
		};

	private:
		attributes_type attributes_;
		js::string_t specifier_;
};

export class js_module {
	public:
		js_module() = delete;
		js_module(const js::iv8::remote_handle_lock& agent, v8::Local<v8::Module> module_);

		auto evaluate(const realm::scope& realm_scope) -> js::value_t;
		auto link(const realm::scope& realm, auto callback) -> void;
		auto requests(const agent_lock& agent) -> std::vector<module_request>;

		static auto compile(const realm::scope& agent, auto&& source_text, source_origin source_origin) -> js_module;
		static auto compile(const agent_lock& agent, auto&& source_text, source_origin source_origin) -> js_module;

		static auto create_synthetic(const realm::scope& realm, auto module_interface, js::string_t origin) -> js_module;

	private:
		static auto compile(const realm::scope& agent, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module;
		static auto create_synthetic(
			const realm::scope& realm,
			std::span<const v8::Local<v8::String>> export_names,
			js::string_t origin,
			synthetic_module_action_type action
		) -> js_module;

		auto link(const realm::scope& realm, v8::Module::ResolveModuleCallback callback) -> void;

		js::iv8::shared_remote<v8::Module> module_;
};

auto js_module::compile(const agent_lock& agent, auto&& source_text, source_origin source_origin) -> js_module {
	// nb: Context lock is needed for compilation errors
	const auto context_lock = js::iv8::context_managed_lock{agent, agent->scratch_context()};
	return compile(realm::scope{context_lock, *agent}, std::forward<decltype(source_text)>(source_text), std::move(source_origin));
}

auto js_module::compile(const realm::scope& realm, auto&& source_text, source_origin source_origin) -> js_module {
	auto local_source_text = js::transfer_in_strict<v8::Local<v8::String>>(std::forward<decltype(source_text)>(source_text), realm);
	return compile(realm, local_source_text, std::move(source_origin));
}

auto js_module::create_synthetic(const realm::scope& realm, auto module_interface, js::string_t origin) -> js_module {
	auto name_locals = std::vector<v8::Local<v8::String>>{};
	auto export_locals = std::vector<v8::Local<v8::Value>>{};
	for (auto& [ key, value ] : module_interface) {
		name_locals.push_back(js::transfer_in_strict<v8::Local<v8::String>>(std::move(key), realm));
		export_locals.push_back(js::transfer_strict<v8::Local<v8::Value>>(std::move(value), std::forward_as_tuple(realm), std::forward_as_tuple(realm)));
	}
	synthetic_module_action_type action =
		[ & ](v8::Local<v8::Context> /*context*/, v8::Local<v8::Module> module) {
			for (const auto& [ key, value ] : std::views::zip(name_locals, export_locals)) {
				js::iv8::unmaybe(module->SetSyntheticModuleExport(realm.isolate(), key, value));
			}
		};
	auto module_record = create_synthetic(realm, std::span{name_locals}, std::move(origin), std::move(action));
	module_record.link(realm, [](auto&&...) -> js_module& { std::unreachable(); });
	module_record.evaluate(realm);
	return module_record;
}

auto js_module::link(const realm::scope& realm, auto callback) -> void {
	// Thread locals are used to hold contextual data about this link request. `InstantiateModule`
	// does not provide any user data, but since it is also a blocking operation it's a good place to
	// put this.
	static thread_local const realm::scope* realm_local;
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
		const auto& realm = *realm_local;
		auto& thread_callback = *thread_callback_local;
		auto specifier_string = js::transfer_out_strict<js::string_t>(specifier, realm);
		auto referrer_name = std::invoke([ & ]() -> std::optional<js::string_t> {
			auto& weak_module_specifiers = realm->weak_module_specifiers();
			const auto* referrer_name = weak_module_specifiers.find(referrer);
			if (referrer_name == nullptr) {
				return std::nullopt;
			} else {
				return *referrer_name;
			}
		});
		auto attributes_view =
			js::iv8::fixed_array{realm.context(), attributes} |
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
		auto attributes_vector = module_request::attributes_type{std::from_range, std::move(attributes_view)};
		auto result = std::invoke([ & ]() -> decltype(auto) {
			const auto unlocker = js::iv8::isolate_unlock{realm};
			return thread_callback(
				std::move(specifier_string),
				std::move(referrer_name),
				std::move(attributes_vector)
			);
		});
		return result.module_.deref(realm);
	};
	link(realm, v8_callback);
}

} // namespace isolated_v8

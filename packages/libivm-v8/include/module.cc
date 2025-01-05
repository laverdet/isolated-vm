module;
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>
export module ivm.isolated_v8:js_module;
import :agent;
import :realm;
import :script;
import ivm.iv8;
import ivm.js;
import ivm.utility;
import v8;

namespace ivm {

export class module_request {
	public:
		using attributes_type = js::dictionary<js::dictionary_tag, js::string_t, js::string_t>;
		module_request(js::string_t specifier, attributes_type attributes_);
		[[nodiscard]] auto specifier() const -> const js::string_t& { return specifier_; }

	private:
		attributes_type attributes_;
		js::string_t specifier_;
};

export class js_module : util::non_copyable {
	public:
		js_module() = delete;
		js_module(agent::lock& agent, v8::Local<v8::Module> module_);

		auto evaluate(realm::scope& realm_scope) -> js::value_t;
		auto link(realm::scope& realm, auto callback) -> void;
		auto requests(agent::lock& agent) -> std::vector<module_request>;

		static auto compile(agent::lock& agent, auto&& source_text, source_origin source_origin) -> js_module;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module;
		auto link(realm::scope& realm, v8::Module::ResolveModuleCallback callback) -> void;

		v8::Global<v8::Module> module_;
};

auto js_module::compile(agent::lock& agent, auto&& source_text, source_origin source_origin) -> js_module {
	v8::Context::Scope context_scope{agent->scratch_context()};
	auto local_source_text = js::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(source_text)>(source_text),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	return js_module::compile(agent, local_source_text, std::move(source_origin));
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
			v8::Local<v8::Context> context,
			v8::Local<v8::String> specifier,
			v8::Local<v8::FixedArray> attributes,
			v8::Local<v8::Module> referrer
		) -> v8::MaybeLocal<v8::Module> {
		auto& realm = *realm_local;
		auto& thread_callback = *thread_callback_local;
		auto* isolate = realm.isolate();
		auto specifier_string = js::transfer_strict<js::string_t>(specifier, std::tuple{isolate, context}, std::tuple{});
		auto referrer_name = std::invoke([ & ]() -> std::optional<js::string_t> {
			const auto* referrer_name = realm.agent()->weak_module_specifiers().find(referrer);
			if (referrer_name == nullptr) {
				return {};
			} else {
				return *referrer_name;
			}
		});
		auto attributes_view =
			js::iv8::fixed_array{attributes, context} |
			// [ key, value, ...[] ]
			std::views::chunk(2) |
			std::views::transform([ & ](const auto& pair) {
				auto visit_args = std::tuple{isolate, context};
				return std::pair{
					js::transfer_strict<js::string_t>(pair[ 0 ].template As<v8::String>(), visit_args, std::tuple{}),
					js::transfer<js::string_t>(pair[ 1 ].template As<v8::Value>(), visit_args, std::tuple{})
				};
			});
		auto attributes_vector = module_request::attributes_type{std::move(attributes_view)};
		auto& result = std::invoke([ & ]() -> decltype(auto) {
			isolate->Exit();
			auto enter = util::scope_exit([ & ]() {
				isolate->Enter();
			});
			v8::Unlocker unlocker{isolate};
			auto&& result = thread_callback(
				std::move(specifier_string),
				std::move(referrer_name),
				std::move(attributes_vector)
			);
			return result;
		});
		return result.module_.Get(isolate);
	};
	link(realm, v8_callback);
}

} // namespace ivm

namespace ivm::js {

template <>
struct object_properties<module_request> {
		constexpr static auto properties = std::tuple{
			accessor<"specifier", &module_request::specifier>{},
		};
};

} // namespace ivm::js

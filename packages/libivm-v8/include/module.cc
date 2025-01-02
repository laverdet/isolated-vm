module;
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

		auto requests(realm::scope& realm) -> std::vector<module_request>;

		static auto compile(realm::scope& realm, auto&& source_text, source_origin source_origin) -> js_module;

	private:
		static auto compile(realm::scope& realm, v8::Local<v8::String> source_text, source_origin source_origin) -> js_module;

		v8::Global<v8::Module> module_;
};

auto js_module::compile(realm::scope& realm, auto&& source_text, source_origin source_origin) -> js_module {
	auto local_source_text = js::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(source_text)>(source_text),
		std::tuple{},
		std::tuple{realm.isolate()}
	);
	return js_module::compile(realm, local_source_text, std::move(source_origin));
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

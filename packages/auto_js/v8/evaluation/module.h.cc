module;
#include <expected>
#include <functional>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
export module v8_js:evaluation.module_record;
import :evaluation.origin;
import :fixed_array;
import :hash;
import :lock;
import :unmaybe;
import :weak_map;
import auto_js;
import util;
import v8;

namespace js::iv8 {

// Data about module `import` statement: `import {} from 'specifier' with { attributes }`
export class module_request {
	public:
		using attributes_type = js::dictionary<js::dictionary_tag, std::u16string, std::u16string>;
		module_request(std::u16string specifier, attributes_type attributes) :
				attributes_{std::move(attributes)},
				specifier_{std::move(specifier)} {}

		[[nodiscard]] auto attributes() const -> const attributes_type& { return attributes_; }
		[[nodiscard]] auto attributes() && -> attributes_type { return std::move(attributes_); }
		[[nodiscard]] auto specifier() const -> const std::u16string& { return specifier_; }
		[[nodiscard]] auto specifier() && -> std::u16string { return std::move(specifier_); }

	private:
		attributes_type attributes_;
		std::u16string specifier_;

	public:
		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"attributes">, &module_request::attributes_},
			js::struct_member{util::cw<"specifier">, &module_request::specifier_},
		};
};

// Information needed to link module record
export struct module_link_record {
		// I guess actually these could be `std::span<T>` but they're coming from a vector anyway.
		std::vector<v8::Local<v8::Module>> modules;
		/** [[ count, module... ]... ] */
		std::vector<unsigned> payload;
};

// `v8::Module` utilities
export class module_record
		: public v8::Local<v8::Module>,
			public handle_with_context {
	public:
		using expected_module_type = std::expected<v8::Local<v8::Module>, js::error_value>;
		using synthetic_module_action_type = auto (*)(context_lock_witness, v8::Local<v8::Module>) -> void;

		explicit module_record(context_lock_witness lock, v8::Local<v8::Module> handle) :
				v8::Local<v8::Module>{handle},
				handle_with_context{lock} {}

		static auto evaluate(context_lock_witness lock, v8::Local<v8::Module> module) -> void;
		static auto link(context_lock_witness lock, v8::Local<v8::Module> module, module_link_record link_record) -> void;
		static auto link(context_lock_witness lock, v8::Local<v8::Module> module) -> void;
		static auto requests(context_lock_witness lock, v8::Local<v8::Module> module) -> std::vector<module_request>;

		static auto compile(context_lock_witness lock, auto source_text, iv8::source_origin origin) -> expected_module_type;
		static auto create_synthetic(context_lock_witness lock, auto module_interface, auto origin) -> v8::Local<v8::Module>;

	private:
		using string_span = std::span<const v8::Local<v8::String>>;

		static auto compile(context_lock_witness lock, v8::Local<v8::String> source_text, iv8::source_origin origin) -> expected_module_type;
		static auto create_synthetic(context_lock_witness lock, string_span export_names, synthetic_module_action_type action, v8::Local<v8::String> module_name) -> v8::Local<v8::Module>;
};

// ---

auto module_record::compile(context_lock_witness lock, auto source_text, iv8::source_origin origin) -> expected_module_type {
	auto local_source_text = js::transfer_in_strict<v8::Local<v8::String>>(std::move(source_text), lock);
	return compile(lock, local_source_text, std::move(origin));
}

auto module_record::create_synthetic(context_lock_witness lock, auto module_interface, auto origin) -> v8::Local<v8::Module> {
	auto origin_local = js::transfer_in_strict<v8::Local<v8::String>>(std::move(origin), lock);
	auto name_locals = std::vector<v8::Local<v8::String>>{};
	auto export_locals = std::vector<v8::Local<v8::Value>>{};
	for (auto& [ key, value ] : module_interface) {
		name_locals.push_back(js::transfer_in_strict<v8::Local<v8::String>>(std::move(key), lock));
		export_locals.push_back(js::transfer_strict<v8::Local<v8::Value>>(std::move(value), std::forward_as_tuple(lock), std::forward_as_tuple(lock)));
	}

	auto callback = [ & ](context_lock_witness lock, v8::Local<v8::Module> module) -> void {
		for (const auto& [ key, value ] : std::views::zip(name_locals, export_locals)) {
			unmaybe(module->SetSyntheticModuleExport(lock.isolate(), key, value));
		}
	};

	using callback_type = decltype(callback);
	thread_local callback_type* tl_callback;
	tl_callback = &callback;

	synthetic_module_action_type action = [](context_lock_witness lock, v8::Local<v8::Module> module) -> void {
		auto& callback = *tl_callback;
		callback(lock, module);
	};
	return create_synthetic(lock, std::span{name_locals}, action, origin_local);
}

} // namespace js::iv8

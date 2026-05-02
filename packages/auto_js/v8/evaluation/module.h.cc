module;
#include "auto_js/gcc_abi_tag.h"
export module v8_js:evaluation.module_record;
import :evaluation.origin;
import :fixed_array;
import :unmaybe;
import :weak_map;
import auto_js;
import std;
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
		GCC_ABI_TAG constexpr static auto struct_template = js::struct_template{
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
export class module_record : public handle_with_context<v8::Module> {
	public:
		using expected_module_type = std::expected<v8::Local<v8::Module>, js::error_value>;
		using expected_value_type = std::expected<std::monostate, js::error_value>;
		using handle_with_context<v8::Module>::handle_with_context;

		static auto evaluate(context_lock_witness lock, v8::Local<v8::Module> module) -> expected_value_type;
		static auto link(context_lock_witness lock, v8::Local<v8::Module> module, module_link_record link_record) -> void;
		static auto link(context_lock_witness lock, v8::Local<v8::Module> module) -> void;
		static auto requests(context_lock_witness lock, v8::Local<v8::Module> module) -> std::vector<module_request>;

		static auto compile(context_lock_witness lock, auto source_text, iv8::source_origin origin) -> expected_module_type;

		template <class... Types>
		static auto create_synthetic(context_lock_witness lock, auto origin, std::tuple<std::in_place_t, Types...> module_interface) -> v8::Local<v8::Module>;

		template <class... Types>
		static auto create_synthetic(context_lock_witness lock, auto origin, std::tuple<Types...> module_interface) -> v8::Local<v8::Module>;

		template <class Type>
		static auto create_synthetic(context_lock_witness lock, auto origin, std::vector<Type> module_interface) -> v8::Local<v8::Module>;

		static auto create_synthetic(
			context_lock_witness lock,
			v8::Local<v8::String> module_name,
			std::span<const v8::Local<v8::String>> export_names,
			std::span<const v8::Local<v8::Value>> export_values
		) -> v8::Local<v8::Module>;

	private:
		static auto compile(context_lock_witness lock, v8::Local<v8::String> source_text, iv8::source_origin origin) -> expected_module_type;
};

// ---

auto module_record::compile(context_lock_witness lock, auto source_text, iv8::source_origin origin) -> expected_module_type {
	auto local_source_text = js::transfer_in_strict<v8::Local<v8::String>>(std::move(source_text), lock);
	return compile(lock, local_source_text, std::move(origin));
}

template <class... Types>
auto module_record::create_synthetic(context_lock_witness lock, auto origin, std::tuple<std::in_place_t, Types...> module_interface) -> v8::Local<v8::Module> {
	auto [... ii ] = util::sequence<sizeof...(Types)>;
	return create_synthetic(lock, origin, std::tuple{std::get<ii + 1>(module_interface)...});
}

template <class... Types>
auto module_record::create_synthetic(context_lock_witness lock, auto origin, std::tuple<Types...> module_interface) -> v8::Local<v8::Module> {
	const auto& [... entries ] = module_interface;
	auto origin_local = js::transfer_in_strict<v8::Local<v8::String>>(std::move(origin), lock);
	auto names = js::transfer_in_strict<std::array<v8::Local<v8::String>, sizeof...(Types)>>(std::tuple{std::get<0>(entries)...}, lock);
	auto exports = std::array{js::transfer_in_strict<v8::Local<v8::Value>>(std::get<1>(entries), lock)...};
	return create_synthetic(lock, origin_local, std::span{names}, std::span{exports});
}

template <class Type>
auto module_record::create_synthetic(context_lock_witness lock, auto origin, std::vector<Type> module_interface) -> v8::Local<v8::Module> {
	auto origin_local = js::transfer_in_strict<v8::Local<v8::String>>(std::move(origin), lock);
	auto name_locals = std::vector<v8::Local<v8::String>>{};
	name_locals.reserve(module_interface.size());
	auto export_locals = std::vector<v8::Local<v8::Value>>{};
	export_locals.reserve(module_interface.size());
	for (auto& [ key, value ] : module_interface) {
		name_locals.push_back(js::transfer_in_strict<v8::Local<v8::String>>(std::move(key), lock));
		export_locals.push_back(js::transfer_in_strict<v8::Local<v8::Value>>(std::move(value), lock));
	}
	return create_synthetic(lock, origin_local, std::span{name_locals}, std::span{export_locals});
}

} // namespace js::iv8

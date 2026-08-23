module;
#include "auto_js/gcc_abi_tag.h"
export module backend_napi_v8:module_;
import :agent_handle;
import :environment;
import :utility;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {
export class module_handle;
export class realm_handle;

struct compile_module_options : js::optional_constructible {
		using js::optional_constructible::optional_constructible;
		std::optional<js::iv8::source_origin> origin;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &compile_module_options::origin},
		};
};

struct create_capability_options {
		std::u16string origin;

		GCC_ABI_TAG constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &create_capability_options::origin},
		};
};

using capability_type = std::variant<forward_callback_type, std::u16string, std::string>;
using capability_interface_type = js::dictionary<js::dictionary_tag, js::string_t, capability_type>;

struct module_handle_link_record {
		std::vector<js::tagged_external<module_handle>> modules;
		std::vector<unsigned> payload;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"modules">, &module_handle_link_record::modules},
			js::struct_member{util::cw<"payload">, &module_handle_link_record::payload},
		};
};

struct remote_module_link_record {
		std::vector<js::iv8::shared_remote<js::iv8::module_record>> modules;
		std::vector<unsigned> payload;
};

export class module_handle {
	public:
		using transfer_type = js::tagged_external<module_handle>;
		module_handle(
			agent_handle agent,
			js::iv8::shared_remote<js::iv8::module_record> module,
			std::optional<std::u16string> specifier = {},
			std::vector<js::iv8::module_request> requests = {}
		);

		auto agent() -> auto& { return agent_; }

		auto evaluate(const environment::lock& lock, realm_handle* realm) -> forward_promise_type;
		auto link(const environment::lock& lock, realm_handle* realm, module_handle_link_record link_record) -> forward_promise_type;
		auto requests(const environment::lock& lock) -> std::vector<js::iv8::module_request>;
		auto specifier(const environment::lock& lock) -> std::optional<std::u16string>;
		static auto class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<module_handle>>;
		static auto compile(const environment::lock& lock, agent_handle& agent, js::string_t source_text, compile_module_options options) -> forward_promise_type;
		static auto create_capability(const environment::lock& lock, realm_handle& realm, capability_interface_type capability_interface, create_capability_options options) -> forward_promise_type;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<js::iv8::module_record> module_;
		std::optional<std::u16string> specifier_;
		std::vector<js::iv8::module_request> requests_;
};

} // namespace backend_napi_v8

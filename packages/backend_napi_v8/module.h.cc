module;
#include <functional>
#include <memory>
#include <optional>
export module backend_napi_v8:module_;
import :agent;
import :environment;
import :realm;
import isolated_js;
import isolated_v8;
import napi_js;
namespace v8 = embedded_v8;

namespace backend_napi_v8 {
using namespace isolated_v8;

struct compile_module_options : js::optional_constructible {
		using js::optional_constructible::optional_constructible;
		std::optional<js::iv8::source_origin> origin;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &compile_module_options::origin},
		};
};

struct create_capability_options {
		std::u16string origin;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"origin">, &create_capability_options::origin},
		};
};

export class subscriber_capability;

export class module_handle {
	private:
		using callback_type = js::forward<js::napi::value<js::function_tag>>;

	public:
		using transfer_type = js::tagged_external<module_handle>;
		module_handle(agent_handle agent, js::iv8::shared_remote<v8::Module> module);

		auto agent() -> auto& { return agent_; }

		auto evaluate(environment& env, realm_handle& realm) -> js::napi::value<promise_tag>;
		auto link(environment& env, realm_handle& realm, callback_type link_callback) -> js::napi::value<promise_tag>;

		static auto compile(agent_handle& agent, environment& env, js::string_t source_text, compile_module_options options) -> js::napi::value<promise_tag>;
		static auto create_capability(realm_handle& realm, environment& env, callback_type make_capability, create_capability_options options) -> js::napi::value<promise_tag>;

		static auto class_template(environment& env) -> js::napi::value<class_tag_of<module_handle>>;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<v8::Module> module_;
};

export class subscriber_capability {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};

	public:
		using callback_type = std::move_only_function<auto(js::value_t) const->bool>;
		using transfer_type = js::tagged_external<subscriber_capability>;
		class subscriber;

		explicit subscriber_capability(private_constructor /*private*/) {};
		auto accept_callback(callback_type callback) -> void;
		auto take_subscriber() -> std::shared_ptr<subscriber>;
		auto send(js::value_t message) -> bool;
		static auto make(environment& env) -> js::napi::value<js::object_tag>;

		static auto class_template(environment& env) -> js::napi::value<js::class_tag_of<subscriber_capability>>;

	private:
		util::lockable<callback_type> callback_;
		std::shared_ptr<subscriber> subscriber_;
};

class subscriber_capability::subscriber {
	public:
		explicit subscriber(const std::shared_ptr<subscriber_capability>& capability);
		auto subscribe(callback_type callback) -> void;

	private:
		std::weak_ptr<subscriber_capability> capability_;
		bool subscribed_ = false;
};

} // namespace backend_napi_v8

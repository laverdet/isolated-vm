export module backend_napi_v8:capability;
import :agent_handle;
import :environment;
import :lock;
import auto_js;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

// Host `Capability` instance, created by static `Capability` factory functions
export class capability_handle : util::non_copyable {
	private:
		// Interface implemented by each kind of capability
		struct instance {
				virtual ~instance() = default;
				// Consumes the instance, moving it into the function template's callback
				[[nodiscard]] virtual auto instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> = 0;
		};

		// `Capability.sync`
		class sync_instance final : public instance {
			public:
				sync_instance(const environment::lock& lock, js::napi::local_of<js::function_tag> callback);
				[[nodiscard]] auto instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> final;

			private:
				class result;

				js::napi::napi_scheduler::ref scheduler_;
				js::napi::unique_remote<js::function_tag> callback_;
		};

		// `Capability.void`
		class void_instance final : public instance {
			public:
				void_instance(const environment::lock& lock, js::napi::local_of<js::function_tag> callback);
				[[nodiscard]] auto instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> final;

			private:
				js::napi::napi_scheduler::ref scheduler_;
				js::napi::shared_remote<js::function_tag> callback_;
		};

	public:
		using instance_type = util::covariant_value<instance, sync_instance, void_instance>;
		using transfer_type = js::tagged_external<capability_handle>;
		using forward_capability_type = js::forward<js::napi::local_of<js::object_tag>>;

		capability_handle(const capability_handle&) = delete;
		capability_handle(capability_handle&& that) noexcept : instance_{std::exchange(that.instance_, std::nullopt)} {}
		~capability_handle() = default;
		auto operator=(const capability_handle&) -> capability_handle& = delete;
		auto operator=(capability_handle&&) -> capability_handle& = delete;
		explicit operator bool() const { return instance_.has_value(); }

		[[nodiscard]] auto instantiate(const agent_lock& lock) && -> v8::Local<v8::FunctionTemplate> {
			return std::move(**instance_).instantiate(lock);
		}

		static auto sync(const environment::lock& lock, forward_callback_type callback) -> forward_capability_type;
		static auto void_(const environment::lock& lock, forward_callback_type callback) -> forward_capability_type;
		static auto class_template(const environment::lock& lock) -> js::napi::local_of<js::class_tag_of<capability_handle>>;

	private:
		explicit capability_handle(instance_type instance) : instance_{std::move(instance)} {}

		std::optional<instance_type> instance_;
};

} // namespace backend_napi_v8

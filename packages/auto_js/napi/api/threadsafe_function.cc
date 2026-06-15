export module napi_js:api.threadsafe_function;
import :api.invoke;
import nodejs;
import std;
import util;

namespace js::napi {

// Maintain a "threadsafe function" via napi. `Context` must be invocable with
// `(napi_env, napi_value, Message&)`.
export template <class Context, class Message>
class threadsafe_function_of : util::non_copyable {
	public:
		using close_callback = util::function_ref<auto()->void>;

		explicit threadsafe_function_of(auto& env, napi_value value, auto&&... args);
		threadsafe_function_of(const threadsafe_function_of&);
		threadsafe_function_of(threadsafe_function_of&&) noexcept;
		~threadsafe_function_of();
		auto operator=(const threadsafe_function_of&) -> threadsafe_function_of& = delete;
		auto operator=(threadsafe_function_of&&) -> threadsafe_function_of& = delete;

		auto close(close_callback callback) -> void;
		auto ref(node_api_basic_env env) const -> void;
		auto unref(node_api_basic_env env) const -> void;
		auto operator()(Message message) const -> bool;
		auto operator*() const -> Context& { return get(); }
		explicit operator bool() const { return tsfn_ != nullptr; }

	private:
		struct storage : Context {
			public:
				friend threadsafe_function_of;
				using Context::Context;

			private:
				close_callback close_hook_{[] -> void {}};
		};
		auto get() const -> storage&;

		constexpr static auto trivial_message =
			std::is_trivially_destructible_v<Message> && sizeof(Message) <= sizeof(void*);
		mutable napi_threadsafe_function tsfn_;
};

// ---

template <class Context, class Message>
threadsafe_function_of<Context, Message>::threadsafe_function_of(auto& env, napi_value value, auto&&... args) :
		tsfn_{[ & ]() -> auto {
			constexpr auto name = util::make_consteval_string_view(util::cw<"threadsafe_function_of">);
			auto resource_name = napi::invoke(napi_create_string_latin1, env, name.data(), name.size());
			auto context = std::make_unique<storage>(std::forward<decltype(args)>(args)...);
			// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
			auto callback = napi_threadsafe_function_call_js{[](napi_env env, napi_value value, void* context_ptr, void* data) -> void {
				auto& context = *static_cast<storage*>(context_ptr);
				if constexpr (trivial_message) {
					if (env != nullptr) {
						auto message = Message{};
						std::memcpy(&message, &data, sizeof(Message));
						context(env, value, message);
					}
				} else {
					auto message = std::unique_ptr<Message>{static_cast<Message*>(data)};
					if (env != nullptr) {
						context(env, value, *message);
					}
				}
			}};
			auto finalize = napi_finalize{[](napi_env /*env*/, void* /*data*/, void* hint) -> void {
				auto* storage_ptr = static_cast<storage*>(hint);
				storage_ptr->close_hook_();
				delete storage_ptr;
			}};
			auto* tsfn = napi::invoke(napi_create_threadsafe_function, env, value, nullptr, resource_name, 0, 1, nullptr, finalize, context.get(), callback);
			context.release();
			return tsfn;
		}()} {
}

template <class Context, class Message>
threadsafe_function_of<Context, Message>::threadsafe_function_of(const threadsafe_function_of& that) :
		tsfn_{that.tsfn_} {
	if (that) {
		napi::invoke0_noexcept(napi_acquire_threadsafe_function, tsfn_);
	}
}

template <class Context, class Message>
threadsafe_function_of<Context, Message>::threadsafe_function_of(threadsafe_function_of&& that) noexcept :
		tsfn_{std::exchange(that.tsfn_, nullptr)} {}

template <class Context, class Message>
threadsafe_function_of<Context, Message>::~threadsafe_function_of() {
	if (*this) {
		napi::invoke0_noexcept(napi_release_threadsafe_function, tsfn_, napi_tsfn_release);
	}
}

template <class Context, class Message>
auto threadsafe_function_of<Context, Message>::close(close_callback callback) -> void {
	get().close_hook_ = callback;
	napi::invoke0_noexcept(napi_release_threadsafe_function, std::exchange(tsfn_, nullptr), napi_tsfn_abort);
}

template <class Context, class Message>
auto threadsafe_function_of<Context, Message>::ref(node_api_basic_env env) const -> void {
	napi::invoke0(napi_ref_threadsafe_function, env, tsfn_);
}

template <class Context, class Message>
auto threadsafe_function_of<Context, Message>::unref(node_api_basic_env env) const -> void {
	napi::invoke0(napi_unref_threadsafe_function, env, tsfn_);
}

template <class Context, class Message>
auto threadsafe_function_of<Context, Message>::operator()(Message message) const -> bool {
	auto check = [ & ](napi_status status) {
		switch (status) {
			case napi_ok:
				return true;
			case napi_queue_full:
				return false;
			case napi_closing:
				// nb: This is undocumented, but if you got this return it means the internal ref count was
				// decremented.
				this->tsfn_ = nullptr;
				return false;
			case napi_invalid_arg:
				std::terminate();
			default:
				std::unreachable();
		}
	};
	if constexpr (trivial_message) {
		void* data{};
		std::memcpy(&data, &message, sizeof(Message));
		return check(napi_call_threadsafe_function(tsfn_, data, napi_tsfn_nonblocking));
	} else {
		auto message_ptr = std::make_unique<Message>(std::move(message));
		auto result = check(napi_call_threadsafe_function(tsfn_, message_ptr.get(), napi_tsfn_nonblocking));
		if (result) {
			message_ptr.release();
		}
		return result;
	}
}

template <class Context, class Message>
auto threadsafe_function_of<Context, Message>::get() const -> storage& {
	auto* context = napi::invoke_noexcept(napi_get_threadsafe_function_context, tsfn_);
	return *reinterpret_cast<storage*>(context);
}

} // namespace js::napi

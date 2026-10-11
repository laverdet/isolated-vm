export module backend_napi_v8:reference;
import :agent_handle;
import :environment;
import :lock;
import :utility;
import auto_js;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {

export class reference_handle {
	public:
		using transfer_type = js::tagged_external<reference_handle>;

		explicit reference_handle(js::null_tag /*tag*/);
		explicit reference_handle(js::undefined_tag /*tag*/);
		reference_handle(agent_handle agent, js::typeof_kind type_of, js::iv8::shared_remote<v8::Context> realm, js::iv8::shared_remote<v8::Value> value);
		reference_handle(const js::iv8::remote_handle_lock& lock, agent_handle agent, js::iv8::shared_remote<v8::Context> realm, v8::Local<v8::Value> value);
		auto copy(const environment::lock& lock) -> forward_promise_type;
		auto get(const environment::lock& lock, js::string_t name) -> forward_promise_type;
		auto set(const environment::lock& lock, js::string_t name, js::forward<js::napi::local_of<>> value_local) -> forward_promise_type;
		auto invoke(const environment::lock& lock, js::forward<js::napi::local_of<list_tag>> params_local, transfer_options options) -> forward_promise_type;

		// Host [nodejs] `Reference` class
		static auto class_template(const environment::lock& lock) -> js::napi::local_of<class_tag_of<reference_handle>>;
		// Runtime [v8] `Reference` class
		static auto class_template(const realm_scope& lock) -> v8::Local<js::iv8::class_template_of<reference_handle>>;

	private:
		agent_handle agent_;
		js::iv8::shared_remote<v8::Context> realm_;
		js::iv8::shared_remote<v8::Value> value_;
		js::typeof_kind typeof_;
};

// Claims a reference found by a visitor delegate as a `std::any`.
template <class Visit, class Accept>
auto claim_reference(auto subject, reference_handle* reference, Visit& visit, const Accept& accept) {
	return reference == nullptr
		? std::nullopt
		: std::optional{[ & ] -> auto {
				auto value = std::any{*reference};
				return accept(js::host_object_tag{}, visit, js::transferred_value{subject, value});
			}()};
}

// Visitor delegate which claims nodejs `Reference` instances
class host_reference_transfer {
	public:
		template <class Visit, class Accept>
		auto operator()(js::napi::local_of<js::object_tag> subject, Visit& visit, const Accept& accept) {
			auto* reference = js::napi::value_of{visit.lock(), subject}.try_cast(std::type_identity<reference_handle>{});
			return claim_reference(napi_value{subject}, reference, visit, accept);
		}

		constexpr auto finalize() -> void {}
};

// Visitor delegate which claims runtime `Reference` instances
class runtime_reference_transfer {
	public:
		template <class Visit, class Accept>
		auto operator()(v8::Local<v8::Object> subject, Visit& visit, const Accept& accept) {
			auto* reference = js::iv8::value_of<js::object_tag>{visit.witness(), subject}.try_cast(std::type_identity<reference_handle>{});
			return claim_reference(v8::Local<v8::Data>{subject}, reference, visit, accept);
		}

		constexpr auto finalize() -> void {}
};

// Acceptor delegate which accepts type-erased `std::any` handles as instances of the receiving
// runtime's `Reference` class.
struct reference_acceptor {
		auto operator()(auto&& subject, const auto& lock) const {
			auto* reference = std::any_cast<reference_handle>(&subject);
			return reference == nullptr
				? std::nullopt
				: std::optional{reference_handle::class_template(lock)->construct(lock, std::forward_like<decltype(subject)>(*reference))};
		}
};

// Accept argument for a value which may hold references
constexpr auto accept_references = js::acceptor_delegate_of{reference_acceptor{}};

// Promise dispatch which transfers a value, which may hold references, into nodejs
constexpr auto resolve_with_references = [](const environment::lock& lock, auto value) -> auto {
	return js::forward{js::transfer_in_strict<napi_value>(std::move(value), lock, accept_references)};
};

// Transfers a value out of the host, claiming listed transferables and `Reference` instances
template <class Type>
auto transfer_from_host(const environment::lock& lock, auto subject, js::napi::value_of<js::list_tag> transfer = {}) -> Type {
	auto delegate = js::visitor_delegate_of{
		js::napi::transfer_list<js::napi::array_buffer_transfer>(lock, transfer),
		host_reference_transfer{},
	};
	return delegate.with([ & ](auto& delegate) -> Type {
		return js::transfer_out<Type>(subject, lock, delegate);
	});
}

// Transfers a value out of the runtime, claiming listed transferables and `Reference` instances
template <class Type>
auto transfer_from_runtime(js::iv8::context_lock_witness lock, auto subject, js::iv8::value_of<js::list_tag> transfer = {}) -> Type {
	auto delegate = js::visitor_delegate_of{
		js::iv8::transfer_list<js::iv8::array_buffer_transfer>(lock, transfer),
		runtime_reference_transfer{},
	};
	return delegate.with([ & ](auto& delegate) -> Type {
		return js::transfer_out<Type>(subject, lock, delegate);
	});
}

} // namespace backend_napi_v8

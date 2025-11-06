module;
#include <utility>
export module v8_js:function;
import isolated_js;
import v8;
import :callback;
import :collected_handle;
import :lock;

namespace js::iv8 {

export struct function_template {
		template <class Agent, class... Implements>
		static auto make(const isolate_lock_witness_of<Agent, Implements...>& lock, auto function) -> v8::Local<v8::FunctionTemplate>;
};

// ---

template <class Agent, class... Implements>
auto function_template::make(const isolate_lock_witness_of<Agent, Implements...>& lock, auto function) -> v8::Local<v8::FunctionTemplate> {
	using lock_type = const context_lock_witness_of<Agent, Implements...>&;
	auto [ callback, data ] = make_callback_storage(lock, make_free_function<lock_type>(std::move(function.callback)));
	return v8::FunctionTemplate::New(lock.isolate(), callback, data, v8::Local<v8::Signature>{}, 0, v8::ConstructorBehavior::kThrow);
}

} // namespace js::iv8

module;
#include <span>
export module napi_js:array_buffer;
import :bound_value;
import :object;
import :utility;
import :value_handle;

namespace js::napi {

// `ArrayBuffer`
class value_for_array_buffer : public value_next<array_buffer_tag> {
	public:
		using value_next<array_buffer_tag>::value_next;
		static auto make(const environment& env, const js::data_block& block) -> value<array_buffer_tag>;
};

class bound_value_for_array_buffer : public bound_value_next<array_buffer_tag> {
	public:
		bound_value_for_array_buffer(const auto& env, value<array_buffer_tag> buffer) :
				bound_value_next{env, buffer} {}

		explicit operator js::data_block() const;
		explicit operator std::span<std::byte>() const;
};

// `SharedArrayBuffer`
class value_for_shared_array_buffer : public value_next<shared_array_buffer_tag> {
	public:
		using value_next<shared_array_buffer_tag>::value_next;
		static auto make(const environment& env, js::data_block block) -> value<shared_array_buffer_tag>;
};

class bound_value_for_shared_array_buffer : public bound_value_next<shared_array_buffer_tag> {
	public:
		bound_value_for_shared_array_buffer(const auto& env, value<shared_array_buffer_tag> buffer) :
				bound_value_next{env, buffer} {}

		explicit operator js::data_block() const;
};

} // namespace js::napi

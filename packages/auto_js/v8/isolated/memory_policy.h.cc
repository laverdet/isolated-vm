export module v8_js:isolated.memory_policy;
import std;
import util;
import v8;

namespace js::iv8::isolated {

export class memory_policy {
	protected:
		~memory_policy() = default;

	public:
		struct create_params;

		// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
		class none;
		// NOLINTNEXTLINE(cppcoreguidelines-virtual-class-destructor)
		class limited;

		virtual auto initialize(v8::Isolate* isolate) -> void = 0;
		virtual auto make_create_params() -> create_params = 0;
		// virtual auto check_soft_limit(v8::Isolate* isolate) -> bool = 0;
		using covariant = util::covariant_value<memory_policy, none, limited>;
};

struct memory_policy::create_params {
		std::shared_ptr<v8::ArrayBuffer::Allocator> allocator;
		v8::ResourceConstraints constraints;
};

class memory_policy::none final : public memory_policy {
	public:
		auto initialize(v8::Isolate* isolate) -> void override {};
		auto make_create_params() -> create_params override;
};

class memory_policy::limited final : public memory_policy {
	public:
		explicit limited(std::size_t memory_limit_bytes) : memory_limit_{memory_limit_bytes} {}
		auto initialize(v8::Isolate* isolate) -> void override;
		auto make_create_params() -> create_params override;

	private:
		std::size_t memory_limit_;
		[[nodiscard]] auto near_heap_limit_callback(std::size_t current_heap_limit, std::size_t initial_heap_limit) const -> std::size_t;
};

} // namespace js::iv8::isolated

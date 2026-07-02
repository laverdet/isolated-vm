module v8_js;
import :isolated.memory_policy;

namespace js::iv8::isolated {

// memory_policy::none
auto memory_policy::none::make_create_params() -> create_params {
	return {
		.allocator = std::shared_ptr<v8::ArrayBuffer::Allocator>(v8::ArrayBuffer::Allocator::NewDefaultAllocator()),
		.constraints = {}
	};
}

// memory_policy::limited
auto memory_policy::limited::initialize(v8::Isolate* isolate) -> void {
	isolate->AddNearHeapLimitCallback(
		// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
		[](void* data, std::size_t current_heap_limit, std::size_t initial_heap_limit) -> std::size_t {
			auto& self = *static_cast<memory_policy::limited*>(data);
			return self.near_heap_limit_callback(current_heap_limit, initial_heap_limit);
		},
		this
	);
}

auto memory_policy::limited::make_create_params() -> create_params {
	v8::ResourceConstraints constraints{};
	constraints.ConfigureDefaultsFromHeapSize(0, memory_limit_);
	return {
		.allocator = std::shared_ptr<v8::ArrayBuffer::Allocator>(v8::ArrayBuffer::Allocator::NewDefaultAllocator()),
		.constraints = constraints,
	};
}

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
auto memory_policy::limited::near_heap_limit_callback(std::size_t current_heap_limit, std::size_t /*initial_heap_limit*/) const -> std::size_t {
	auto* isolate = v8::Isolate::GetCurrent();
	v8::HeapStatistics heap;
	isolate->GetHeapStatistics(&heap);

	if (current_heap_limit > memory_limit_) {
		auto& executor = agent_host::get_current(isolate).executor();
		executor.terminate();
	}
	// Increase heap limit 64mb at a time
	return current_heap_limit + (64 << 20);
}

} // namespace js::iv8::isolated

module;
#include <cassert>
#include <memory>
module v8_js;

namespace js::iv8::isolated {

auto cluster::acquire_agent_storage() -> std::shared_ptr<agent_storage> {
	auto storage = std::make_shared<agent_storage>(*this);
	agent_storage_.write()->push_back(*storage);
	return storage;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
auto cluster::release_agent_storage(std::shared_ptr<agent_storage> storage) -> void {
	agent_storage_.write()->erase(agent_storage_list_type::s_iterator_to(*storage));
	// I haven't seen this case yet.
	assert(!storage->foreground_runner().scheduler().is_this_thread());
}

} // namespace js::iv8::isolated

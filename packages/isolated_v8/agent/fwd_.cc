module;
#include <optional>
export module isolated_v8:agent_fwd;
import :clock;

namespace isolated_v8 {

export class agent_handle;
export class agent_host;
export class agent_severable;

export struct behavior_params {
		clock::any_clock clock;
		std::optional<double> random_seed;
};

} // namespace isolated_v8

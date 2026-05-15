#include "node_version.h"
static_assert(NAPI_VERSION <= NODE_API_SUPPORTED_VERSION_MAX, "NAPI version mismatch");

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define NODE_EQ_VERSION_AT_LEAST(major, minor, patch)                 \
	(((major) == NODE_MAJOR_VERSION && (minor) < NODE_MINOR_VERSION) || \
	 ((major) == NODE_MAJOR_VERSION && (minor) == NODE_MINOR_VERSION && (patch) <= NODE_PATCH_VERSION))

static_assert(NAPI_VERSION >= 10);
static_assert(NODE_VERSION_AT_LEAST(23, 6, 0) || NODE_EQ_VERSION_AT_LEAST(22, 14, 0));

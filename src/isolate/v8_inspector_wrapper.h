#pragma once
#include "node_wrapper.h"
#if NODE_MODULE_VERSION >= 83
#include "v8_inspector/nodejs_v14.0.0.h"
#elif NODE_MODULE_VERSION >= 67
#include "v8_inspector/nodejs_v11.0.0.h"
#else
#include <v8-inspector.h>
#endif

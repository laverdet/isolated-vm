#pragma once
#include "node_wrapper.h"
#include "./v8_version.h"
#if V8_AT_LEAST(10, 2, 154)
#include "v8_inspector/nodejs_v18.3.0.h"
#elif V8_AT_LEAST(10, 1, 124)
#include "v8_inspector/nodejs_v18.0.0.h"
#elif NODE_MODULE_VERSION >= 93
#if V8_AT_LEAST(9, 4, 146)
#include "v8_inspector/nodejs_v16.11.0.h"
#else
#include "v8_inspector/nodejs_v16.0.0.h"
#endif
#else
#include <v8-inspector.h>
#endif

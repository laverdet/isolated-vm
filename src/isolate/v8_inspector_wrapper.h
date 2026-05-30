#pragma once
#include "node_wrapper.h"
#include "./v8_version.h"
#if V8_AT_LEAST(14, 6, 202)
#include "v8_inspector/nodejs_v26.0.0.h"
#elif V8_AT_LEAST(13, 6, 233)
#include "v8_inspector/nodejs_v24.0.0.h"
#elif V8_AT_LEAST(12, 4, 254)
#include "v8_inspector/nodejs_v22.0.0.h"
#endif

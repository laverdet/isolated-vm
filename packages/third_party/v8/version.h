#include <v8-version.h>

#define V8_AT_LEAST(major, minor, patch) (                                                   \
	V8_MAJOR_VERSION > (major) ||                                                              \
	(V8_MAJOR_VERSION == (major) && V8_MINOR_VERSION > (minor)) ||                             \
	(V8_MAJOR_VERSION == (major) && V8_MINOR_VERSION == (minor) && V8_BUILD_NUMBER >= (patch)) \
)

// https://github.com/v8/v8/commit/4f8d4447533bc8d119048961c2f103d9875674e0
#define V8_HAS_NUMERIC V8_AT_LEAST(11, 7, 352)

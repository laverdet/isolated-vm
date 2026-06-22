#if __linux__
#define V8_EXPORT __attribute__((weak))
#elif __APPLE__
#define V8_EXPORT __attribute__((weak))
#elif _WIN64
#define V8_EXPORT __declspec(dllimport) __attribute__((weak))
#else
#define V8_EXPORT
#endif

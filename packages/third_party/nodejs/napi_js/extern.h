#if __linux__
#define NAPI_EXTERN __attribute__((weak))
#elif __APPLE__
#define NAPI_EXTERN __attribute__((weak))
#elif _WIN64
#define NAPI_EXTERN __declspec(dllimport) __attribute__((weak))
#else
#define NAPI_EXTERN
#endif
#define UV_EXTERN NAPI_EXTERN

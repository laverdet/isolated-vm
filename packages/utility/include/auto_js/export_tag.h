#if EXPORT_IS_EXPORT

#if _WIN64
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

#else

#if _WIN64
#define EXPORT __declspec(dllimport)
#else
#define EXPORT
#endif

#endif

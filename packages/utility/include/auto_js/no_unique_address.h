// See: https://devblogs.microsoft.com/cppblog/msvc-cpp20-and-the-std-cpp20-switch/
// But the main reason this is a macro is because clang-format clobbers the msvc attribute for some
// reason.
#if _MSC_VER
#define NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define NO_UNIQUE_ADDRESS [[no_unique_address]]
#endif

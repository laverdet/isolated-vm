#ifdef __GNUC__
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=124957
#define GCC_ABI_TAG [[gnu::abi_tag("cxx11")]]
#else
#define GCC_ABI_TAG
#endif

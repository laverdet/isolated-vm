#include <__functional/hash.h>
#if _LIBCPP_AVAILABILITY_HAS_HASH_MEMORY
namespace std::inline __1 {
[[__gnu__::__pure__]] _LIBCPP_EXPORTED_FROM_ABI size_t __hash_memory(_LIBCPP_NOESCAPE const void* __ptr, size_t __size) _NOEXCEPT {
	return __murmur2_or_cityhash<size_t>()(__ptr, __size);
}
} // namespace std::inline __1
#endif

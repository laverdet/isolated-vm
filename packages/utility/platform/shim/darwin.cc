#include <__functional/hash.h>
namespace std::inline __1 {
auto __hash_memory([[_Clang::__noescape__]] const void *ptr, size_t size) noexcept -> size_t {
  return __murmur2_or_cityhash<size_t>()(ptr, size);
}
} // namespace std::inline __1

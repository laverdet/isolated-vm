#pragma once
#include <string>

namespace ivm {

/**
 * Number of whole CPUs this process's cgroup allows it to use, rounded up, or 0 when there is no
 * limit (including on non-Linux platforms). `sysroot` prefixes /proc and the cgroup mount points
 * so the resolution can be exercised against fixtures.
 */
auto cgroup_cpu_limit(const std::string& sysroot = {}) -> unsigned;

} // namespace ivm

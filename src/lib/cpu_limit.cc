#include "cpu_limit.h"
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ivm {
namespace {

auto parse_ll(const std::string& text, long long& out) -> bool {
	if (text.empty()) {
		return false;
	}
	errno = 0;
	char* end = nullptr;
	long long value = std::strtoll(text.c_str(), &end, 10);
	if (end == text.c_str() || *end != '\0' || errno == ERANGE) {
		return false;
	}
	out = value;
	return true;
}

auto ceil_div(long long quota, long long period) -> unsigned {
	if (quota <= 0 || period <= 0) {
		return 0;
	}
	return static_cast<unsigned>((quota + period - 1) / period);
}

// 0 means "no limit", so it loses to any real limit.
auto tighter(unsigned lhs, unsigned rhs) -> unsigned {
	if (lhs == 0) {
		return rhs;
	}
	if (rhs == 0) {
		return lhs;
	}
	return lhs < rhs ? lhs : rhs;
}

// Is `name` one of the comma separated entries in `list`?
auto has_entry(const std::string& list, const std::string& name) -> bool {
	size_t pos = 0;
	while (true) {
		size_t next = list.find(',', pos);
		size_t len = (next == std::string::npos ? list.size() : next) - pos;
		if (list.compare(pos, len, name) == 0) {
			return true;
		}
		if (next == std::string::npos) {
			return false;
		}
		pos = next + 1;
	}
}

// cgroup v2 writes "<quota> <period>", or "max <period>" when unconstrained.
auto read_v2(const std::string& dir) -> unsigned {
	std::ifstream file{dir + "/cpu.max"};
	std::string quota;
	std::string period;
	if (!(file >> quota >> period)) {
		return 0;
	}
	long long quota_value = 0;
	long long period_value = 0;
	if (!parse_ll(quota, quota_value) || !parse_ll(period, period_value)) {
		return 0;
	}
	return ceil_div(quota_value, period_value);
}

// cgroup v1 splits it across two files and writes -1 when unconstrained.
auto read_v1(const std::string& dir) -> unsigned {
	std::ifstream quota_file{dir + "/cpu.cfs_quota_us"};
	std::ifstream period_file{dir + "/cpu.cfs_period_us"};
	std::string quota;
	std::string period;
	if (!(quota_file >> quota) || !(period_file >> period)) {
		return 0;
	}
	long long quota_value = 0;
	long long period_value = 0;
	if (!parse_ll(quota, quota_value) || !parse_ll(period, period_value)) {
		return 0;
	}
	return ceil_div(quota_value, period_value);
}

struct mount_t {
	std::string root;
	std::string point;
	bool v2 = false;
	bool found = false;
};

// mountinfo lines are "<id> <parent> <maj:min> <root> <point> <opts>... - <type> <src> <superopts>".
// The controller list for v1 lives in the super options, where "cpu" and "cpuacct" are commonly
// mounted together as a single "cpu,cpuacct" hierarchy.
auto find_cpu_mount(const std::string& sysroot) -> mount_t {
	std::ifstream file{sysroot + "/proc/self/mountinfo"};
	std::string line;
	mount_t v1;
	while (std::getline(file, line)) {
		std::istringstream stream{line};
		std::vector<std::string> head;
		std::string field;
		while (stream >> field && field != "-") {
			head.push_back(field);
		}
		if (head.size() < 5) {
			continue;
		}
		std::string type;
		std::string source;
		std::string super;
		stream >> type >> source >> super;
		if (type == "cgroup2") {
			return mount_t{head[3], head[4], true, true};
		}
		if (type == "cgroup" && !v1.found && has_entry(super, "cpu")) {
			v1 = mount_t{head[3], head[4], false, true};
		}
	}
	return v1;
}

// /proc/self/cgroup lines are "<hierarchy-id>:<controllers>:<path>"; v2 uses an empty controller
// field.
auto find_cgroup_path(const std::string& sysroot, bool v2) -> std::string {
	std::ifstream file{sysroot + "/proc/self/cgroup"};
	std::string line;
	while (std::getline(file, line)) {
		size_t first = line.find(':');
		if (first == std::string::npos) {
			continue;
		}
		size_t second = line.find(':', first + 1);
		if (second == std::string::npos) {
			continue;
		}
		std::string controllers = line.substr(first + 1, second - first - 1);
		if (v2 ? controllers.empty() : has_entry(controllers, "cpu")) {
			return line.substr(second + 1);
		}
	}
	return "/";
}

} // anonymous namespace

// Not guarded by __linux__: on other platforms /proc/self/mountinfo simply does not open and we
// fall out with "no limit", at the cost of one failed open during static init.
auto cgroup_cpu_limit(const std::string& sysroot) -> unsigned {
	mount_t mount = find_cpu_mount(sysroot);
	if (!mount.found) {
		return 0;
	}

	// When only a subtree is mounted -- which is what a container namespace does -- the path from
	// /proc/self/cgroup is still written relative to the hierarchy root, so strip the mounted
	// prefix off it before joining.
	std::string cgroup = find_cgroup_path(sysroot, mount.v2);
	if (mount.root != "/" && cgroup.rfind(mount.root, 0) == 0) {
		cgroup = cgroup.substr(mount.root.size());
	}
	if (cgroup.empty() || cgroup.front() != '/') {
		cgroup.insert(cgroup.begin(), '/');
	}

	// An ancestor can be more restrictive than the group the process sits in, and in a namespaced
	// container the leaf path may not exist under the mount at all, so collect every level from
	// the leaf up to the mount point and take the tightest limit any of them declares.
	std::string base = sysroot + mount.point;
	std::string dir = base + (cgroup == "/" ? std::string{} : cgroup);
	std::vector<std::string> dirs;
	while (dir.size() > base.size()) {
		dirs.push_back(dir);
		size_t pos = dir.find_last_of('/');
		if (pos == std::string::npos || pos < base.size()) {
			break;
		}
		dir.resize(pos);
	}
	dirs.push_back(base);

	unsigned limit = 0;
	for (const std::string& candidate : dirs) {
		limit = tighter(limit, mount.v2 ? read_v2(candidate) : read_v1(candidate));
	}
	return limit;
}

} // namespace ivm

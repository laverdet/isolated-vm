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

// mountinfo escapes space, tab, newline and backslash in the root, mountpoint and source fields
// as a backslash followed by three octal digits, so those fields must be decoded before use as
// paths. See mangle() in fs/proc_namespace.c.
auto unescape(const std::string& text) -> std::string {
	std::string out;
	out.reserve(text.size());
	for (size_t ii = 0; ii < text.size(); ++ii) {
		bool octal = text[ii] == '\\' && ii + 3 < text.size();
		for (size_t jj = 1; octal && jj <= 3; ++jj) {
			octal = text[ii + jj] >= '0' && text[ii + jj] <= '7';
		}
		if (octal) {
			int value = (text[ii + 1] - '0') * 64 + (text[ii + 2] - '0') * 8 + (text[ii + 3] - '0');
			out.push_back(static_cast<char>(value));
			ii += 3;
		} else {
			out.push_back(text[ii]);
		}
	}
	return out;
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
};

// mountinfo lines are "<id> <parent> <maj:min> <root> <point> <opts>... - <type> <src> <superopts>".
// The controller list for v1 lives in the super options, where "cpu" and "cpuacct" are commonly
// mounted together as a single "cpu,cpuacct" hierarchy.
//
// Every cgroup mount is collected rather than just the first of each kind. A hybrid system mounts
// cgroup2 alongside the v1 controllers and cpu may well still be the v1 one, and the same
// hierarchy can be mounted more than once -- including binds of a subtree this process is not in,
// which must not be mistaken for its own.
auto find_cpu_mounts(const std::string& sysroot) -> std::vector<mount_t> {
	std::ifstream file{sysroot + "/proc/self/mountinfo"};
	std::string line;
	std::vector<mount_t> mounts;
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
			mounts.push_back(mount_t{unescape(head[3]), unescape(head[4]), true});
		} else if (type == "cgroup" && has_entry(super, "cpu")) {
			mounts.push_back(mount_t{unescape(head[3]), unescape(head[4]), false});
		}
	}
	return mounts;
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

// Does this mount expose the group the process is actually in? Compared on whole path components,
// so a mount rooted at "/other" neither contains "/app" nor "/otherthing".
auto contains_path(const std::string& root, const std::string& path) -> bool {
	if (root.empty() || root == "/") {
		return true;
	}
	if (path.rfind(root, 0) != 0) {
		return false;
	}
	return path.size() == root.size() || path[root.size()] == '/';
}

// The process's group expressed relative to the mount root, which is what the mount exposes.
auto relative_to(const std::string& root, const std::string& path) -> std::string {
	std::string rest = (root.empty() || root == "/") ? path : path.substr(root.size());
	if (rest.empty() || rest.front() != '/') {
		rest.insert(rest.begin(), '/');
	}
	return rest;
}

// Tightest limit declared anywhere along the process's own path within this mount, or 0 for none.
auto limit_for_mount(const std::string& sysroot, const mount_t& mount, const std::string& relative)
		-> unsigned {
	// An ancestor can be more restrictive than the group the process sits in, so read every level
	// from the leaf up to the mount point. Only levels on the process's verified path are read.
	std::string base = sysroot + mount.point;
	std::string dir = base + (relative == "/" ? std::string{} : relative);
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

} // anonymous namespace

// Not guarded by __linux__: on other platforms /proc/self/mountinfo simply does not open and we
// fall out with "no limit", at the cost of one failed open during static init.
auto cgroup_cpu_limit(const std::string& sysroot) -> unsigned {
	std::string v2_path = find_cgroup_path(sysroot, true);
	std::string v1_path = find_cgroup_path(sysroot, false);
	unsigned limit = 0;
	for (const mount_t& mount : find_cpu_mounts(sysroot)) {
		const std::string& path = mount.v2 ? v2_path : v1_path;
		// A mount rooted somewhere this process does not live belongs to another workload; reading
		// its quota would size this pool from someone else's limit.
		if (!contains_path(mount.root, path)) {
			continue;
		}
		limit = tighter(limit, limit_for_mount(sysroot, mount, relative_to(mount.root, path)));
	}
	return limit;
}

} // namespace ivm

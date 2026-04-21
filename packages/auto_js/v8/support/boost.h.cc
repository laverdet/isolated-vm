module;
// nb: Simply including this header causes "not defined" errors in gcc. Shimming as a module fixes
// it.
#include <boost/intrusive/list.hpp>
export module v8_js:boost;

// NOLINTBEGIN(misc-unused-using-decls)
export namespace boost::intrusive {
using intrusive::constant_time_size;
using intrusive::link_mode;
using intrusive::list;
using intrusive::list_member_hook;
using intrusive::member_hook;
using intrusive::safe_link;

namespace detail {
using detail::destructor_impl;
}

} // namespace boost::intrusive
// NOLINTEND(misc-unused-using-decls)

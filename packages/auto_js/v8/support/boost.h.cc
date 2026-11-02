module;
// nb: Simply including this header causes "not defined" errors in gcc. Shimming as a module fixes
// it.
#include <boost/intrusive/list.hpp>
#include <boost/intrusive/slist.hpp>
export module v8_js:boost;

// NOLINTBEGIN(misc-unused-using-decls)
export namespace boost::intrusive {
using intrusive::constant_time_size;
using intrusive::link_mode;
using intrusive::list;
using intrusive::list_member_hook;
using intrusive::member_hook;
using intrusive::safe_link;
using intrusive::slist;
using intrusive::slist_member_hook;

namespace detail {
using detail::destructor_impl;
}

} // namespace boost::intrusive
// NOLINTEND(misc-unused-using-decls)

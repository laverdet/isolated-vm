module;
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
export module ivm.value:dictionary.accept;
import :dictionary.helpers;
import :dictionary.vector_of;
import :tag;
import :transfer;
import ivm.utility;

namespace ivm::value {

// If the container is not recursive then it will own its own entry acceptor. Otherwise it accepts
// a reference to an existing one.
template <class Meta, class Type>
struct accept<Meta, entry_subject<Type>> : accept_next<Meta, Type> {
		constexpr accept(const auto_visit auto& visit) :
				accept_next<Meta, Type>{visit} {}
		constexpr accept(int /*dummy*/, const auto_visit auto& visit, const auto_accept auto& /*accept*/) :
				accept{visit} {}
};

template <class Meta, class Type>
	requires is_recursive_v<Type>
struct accept<Meta, entry_subject<Type>> {
	private:
		using accept_type = accept_next<Meta, Type>;

	public:
		// nb: No `auto_visit` constructor because this is the recursive case and we require a reference
		constexpr accept(int /*dummy*/, const auto_visit auto& /*visit*/, const auto_accept auto& accept) :
				accept_{&accept} {}

		constexpr auto operator()(auto_tag auto tag, auto&&... args) const -> decltype(auto)
			requires std::invocable<accept_type, decltype(tag), decltype(args)...> {
			return (*accept_)(tag, std::forward<decltype(args)>(args)...);
		}

	private:
		const accept_type* accept_;
};

// Dictionary's acceptor manages the recursive acceptor for the entry key/value types
template <class Meta, class Tag, class Entry, class Subject>
struct accept<Meta, vector_of_subject<Tag, Entry, Subject>> {
	public:
		constexpr accept(const auto_visit auto& visit) :
				accept_{visit} {}
		constexpr accept(int dummy, const auto_visit auto& visit, const auto_accept auto& accept) :
				accept_{dummy, visit, accept} {}

		constexpr auto operator()(Tag /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			return vector_of<Tag, Entry>{
				util::into_range(std::forward<decltype(dictionary)>(dictionary)) |
				std::views::transform([ & ](auto entry) {
					auto&& [ key, value ] = entry;
					return std::pair{
						visit.first(std::forward<decltype(key)>(key), accept_.first),
						visit.second(std::forward<decltype(value)>(value), accept_.second)
					};
				})
			};
		}

		template <std::size_t Size>
			requires std::is_same_v<dictionary_tag, Tag>
		constexpr auto operator()(struct_tag<Size> /*tag*/, auto&& dictionary, const auto& visit) const -> vector_of<Tag, Entry> {
			return std::invoke(
				[]<size_t... Index>(const auto& invoke, std::index_sequence<Index...> /*indices*/) constexpr {
					vector_of<Tag, Entry>{invoke(std::integral_constant<size_t, Index>{})...};
				},
				[ & ]<std::size_t Index>(std::integral_constant<size_t, Index> /*index*/) constexpr {
					const auto& visit_n = std::get<Index>(visit);
					accept<void, accept_immediate<string_tag>> accept_string;
					return std::pair{
						visit_n.first(accept_, accept_string),
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						visit_n.second(std::forward<decltype(dictionary)>(dictionary), accept_),
					};
				},
				std::make_index_sequence<Size>{}
			);
		}

	private:
		accept<Meta, Subject> accept_;
};

// Entrypoint for `vector_of` accept
template <class Meta, class Tag, class Entry>
struct accept<Meta, vector_of<Tag, Entry>>
		: accept<Meta, vector_of_subject<Tag, Entry, entry_subject_for_t<Entry>>> {
		using accept<Meta, vector_of_subject<Tag, Entry, entry_subject_for_t<Entry>>>::accept;
};

} // namespace ivm::value

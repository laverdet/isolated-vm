module;
#include "auto_js/export_tag.h"
export module isolated_vm:value.record;
import :value.vector;

namespace isolated_vm {
using namespace js;

// value_of<record_tag>
class EXPORT value_for_record : public value_next<record_tag> {
	public:
		using value_next<record_tag>::value_next;
		auto set(const runtime_lock& lock, value_of<> key, value_of<> value) const -> void;
		static auto make(const runtime_lock& lock) -> value_of<record_tag>;
};

// bound_value<record_tag>
class EXPORT bound_value_for_record : public bound_value_next<record_tag> {
	private:
		using internal_keys_type = bound_value<vector_tag>;

	public:
		using bound_value_next<record_tag>::bound_value_next;
		using key_type = value_of<primitive_tag>;
		using mapped_type = value_of<>;
		using value_type = std::pair<key_type, mapped_type>;

	private:
		class EXPORT iterator_transform {
			public:
				explicit iterator_transform(const bound_value_for_record& subject);
				auto operator()(value_of<> key) const -> value_type;

			private:
				const bound_value_for_record* subject_;
		};

	public:
		using range_type = std::ranges::transform_view<std::views::all_t<const internal_keys_type&>, iterator_transform>;
		using iterator = std::ranges::iterator_t<range_type>;

		[[nodiscard]] auto get(value_of<name_tag> key) const -> value_of<>;
		[[nodiscard]] auto get(value_of<number_tag> key) const -> value_of<>;
		[[nodiscard]] auto get(value_of<primitive_tag> key) const -> value_of<>;
		[[nodiscard]] auto has(value_of<name_tag> key) const -> bool;
		[[nodiscard]] auto has(value_of<number_tag> key) const -> bool;
		[[nodiscard]] auto has(value_of<primitive_tag> key) const -> bool;
		[[nodiscard]] auto into_range() const -> range_type;
		[[nodiscard]] auto size() const -> std::size_t;

	private:
		[[nodiscard]] auto keys() const -> const internal_keys_type&;

		mutable internal_keys_type keys_;
};

} // namespace isolated_vm

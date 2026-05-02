export module isolated_vm:transfer.visit;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

struct visit_vm_value {
	public:
		explicit visit_vm_value(const runtime_lock& lock) : lock_{lock} {}

		template <class Accept>
		auto operator()(value_of<value_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::undefined: return accept_as(undefined_tag{}, subject, accept);
				case value_typeof::null: return accept_as(null_tag{}, subject, accept);
				case value_typeof::boolean: return accept_as(boolean_tag{}, subject, accept);
				case value_typeof::number: return accept_as(number_tag{}, subject, accept);
				case value_typeof::number_i32: return accept_as(number_tag_of<std::int32_t>{}, subject, accept);
				case value_typeof::string: return accept_as(string_tag_of<char16_t>{}, subject, accept);
				case value_typeof::string_latin1: return accept_as(string_tag_of<char>{}, subject, accept);
				case value_typeof::symbol: return accept_as(symbol_tag{}, subject, accept);
				case value_typeof::bigint: return accept_as(bigint_tag{}, subject, accept);
				case value_typeof::bigint_i64: return accept_as(bigint_tag_of<std::int64_t>{}, subject, accept);
			}
			std::unreachable();
		}

		template <class Accept>
		auto operator()(value_of<primitive_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::undefined: return accept_as(undefined_tag{}, subject, accept);
				case value_typeof::null: return accept_as(null_tag{}, subject, accept);
				case value_typeof::boolean: return accept_as(boolean_tag{}, subject, accept);
				case value_typeof::number: return accept_as(number_tag{}, subject, accept);
				case value_typeof::number_i32: return accept_as(number_tag_of<std::int32_t>{}, subject, accept);
				case value_typeof::string: return accept_as(string_tag_of<char16_t>{}, subject, accept);
				case value_typeof::string_latin1: return accept_as(string_tag_of<char>{}, subject, accept);
				case value_typeof::symbol: return accept_as(symbol_tag{}, subject, accept);
				case value_typeof::bigint: return accept_as(bigint_tag{}, subject, accept);
				case value_typeof::bigint_i64: return accept_as(bigint_tag_of<std::int64_t>{}, subject, accept);
			}
			std::unreachable();
		}

		template <class Accept>
		auto operator()(value_of<number_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::number: return accept_as(number_tag{}, subject, accept);
				case value_typeof::number_i32: return accept_as(number_tag_of<std::int32_t>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		auto operator()(value_of<name_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::string: return accept_as(string_tag_of<char16_t>{}, subject, accept);
				case value_typeof::string_latin1: return accept_as(string_tag_of<char>{}, subject, accept);
				case value_typeof::symbol: return accept_as(symbol_tag{}, subject, accept);
				default: std::unreachable();
			}
			std::unreachable();
		}

		template <class Accept>
		auto operator()(value_of<string_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::string: return accept_as(string_tag_of<char16_t>{}, subject, accept);
				case value_typeof::string_latin1: return accept_as(string_tag_of<char>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		auto operator()(value_of<bigint_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (inspect_type(lock_, subject)) {
				case value_typeof::bigint: return accept_as(bigint_tag{}, subject, accept);
				case value_typeof::bigint_i64: return accept_as(bigint_tag_of<std::int64_t>{}, subject, accept);
				default: std::unreachable();
			}
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		template <class Accept, class Tag>
		auto accept_as(Tag tag, value_of<value_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(tag, *this, bound_value{lock_, value_of<Tag>::from(subject)});
		}

		std::reference_wrapper<const runtime_lock> lock_;
};

} // namespace isolated_vm

namespace js {
using namespace isolated_vm;

template <class Tag>
struct visit<void, value_of<Tag>> : visit_vm_value {
		using visit_vm_value::visit_vm_value;
};

} // namespace js

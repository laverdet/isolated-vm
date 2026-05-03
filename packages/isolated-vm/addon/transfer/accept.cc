module;
#include "auto_js/export_tag.h"
#include <concepts>
export module isolated_vm:transfer.accept;
import :support.free_function;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

struct EXPORT accept_vm_primitive {
	public:
		explicit accept_vm_primitive(const basic_lock& lock) : lock_{lock} {}

		// undefined
		auto operator()(undefined_tag tag, visit_holder visit, std::monostate subject) const -> value_of<undefined_tag>;
		auto operator()(undefined_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<undefined_tag> {
			return (*this)(tag, visit, std::monostate{});
		}

		// null
		auto operator()(null_tag tag, visit_holder visit, std::nullptr_t subject) const -> value_of<null_tag>;
		auto operator()(null_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<null_tag> {
			return (*this)(tag, visit, nullptr);
		}

		// boolean
		auto operator()(boolean_tag tag, visit_holder visit, bool subject) const -> value_of<boolean_tag>;

		// number
		auto operator()(number_tag_of<double> tag, visit_holder visit, double subject) const -> value_of<number_tag_of<double>>;
		auto operator()(number_tag_of<std::int32_t> tag, visit_holder visit, std::int32_t) const -> value_of<number_tag_of<std::int32_t>>;
		auto operator()(number_tag_of<std::uint32_t> tag, visit_holder visit, std::uint32_t) const -> value_of<number_tag_of<std::uint32_t>>;

		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<double>> {
			return (*this)(number_tag_of<double>{}, visit, double{std::forward<decltype(subject)>(subject)});
		}

		template <class Type>
		auto operator()(number_tag_of<Type> tag, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<Type>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// string
		auto operator()(string_tag_of<char> tag, visit_holder visit, std::string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char>>>;
		auto operator()(string_tag_of<char16_t> tag, visit_holder visit, std::u16string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char16_t>>>;
		auto operator()(string_tag_of<char8_t> tag, visit_holder visit, std::u8string_view subject) const
			-> js::referenceable_value<value_of<string_tag>>;

		auto operator()(string_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag>> {
			auto value = (*this)(string_tag_of<char16_t>{}, visit, std::u16string_view{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<string_tag>{*value}};
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, std::convertible_to<std::basic_string_view<Char>> auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string_view<Char>{std::forward<decltype(subject)>(subject)});
		}

		template <class Char>
		auto operator()(string_tag_of<Char> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<string_tag_of<Char>>> {
			return (*this)(tag, visit, std::basic_string<Char>{std::forward<decltype(subject)>(subject)});
		}

		// bigint
		auto operator()(bigint_tag_of<std::int64_t> tag, visit_holder visit, std::int64_t subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<std::int64_t>>>;
		auto operator()(bigint_tag_of<std::uint64_t> tag, visit_holder visit, std::uint64_t subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<std::uint64_t>>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, const js::bigint& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>>;
		auto operator()(bigint_tag_of<js::bigint> tag, visit_holder visit, js::bigint&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<js::bigint>>>;

		auto operator()(bigint_tag /*tag*/, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag>> {
			auto value = (*this)(bigint_tag_of<js::bigint>{}, visit, js::bigint{std::forward<decltype(subject)>(subject)});
			return js::referenceable_value{value_of<bigint_tag>{*value}};
		}

		template <class Type>
		auto operator()(bigint_tag_of<Type> tag, visit_holder visit, auto&& subject) const
			-> js::referenceable_value<value_of<bigint_tag_of<Type>>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		[[nodiscard]] auto lock() const -> const basic_lock& { return lock_; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		std::reference_wrapper<const basic_lock> lock_;
};

struct EXPORT accept_vm_prototype : accept_vm_primitive {
	public:
		using accept_vm_primitive::accept_vm_primitive;
		using accept_vm_primitive::lock;

		// cast primitive to prototype
		template <std::convertible_to<primitive_tag> Tag>
		auto operator()(Tag tag, auto& visit, auto&& subject) const -> value_of<prototype_tag> {
			const accept_vm_primitive& accept = *this;
			auto result = accept(tag, visit, std::forward<decltype(subject)>(subject));
			constexpr auto unwrap = util::overloaded{
				[](auto value) { return value; },
				[]<class Type>(js::referenceable_value<Type> value) { return *std::move(value); },
			};
			return value_of<prototype_tag>::from(unwrap(result));
		}

		// function
		auto operator()(function_prototype_tag /*tag*/, visit_holder /*visit*/, auto subject) const -> value_of<function_prototype_tag> {
			using lock_type = const runtime_lock&;
			auto [ function, length ] = make_free_function<lock_type>(std::move(subject).callback);
			auto data = make_callback_storage(std::move(function));
			return make_function_prototype(std::move(data), length);
		}

	private:
		// function
		[[nodiscard]] auto make_function_prototype(runtime_callback_data_span_type data, int length) const -> value_of<function_prototype_tag>;
		[[nodiscard]] auto make_function_prototype(runtime_callback_data_allocated_type data, int length) const -> value_of<function_prototype_tag>;

		template <class Type>
		[[nodiscard]] auto make_function_prototype(runtime_callback_function_storage<Type> data, int length) const -> value_of<function_prototype_tag> {
			auto data_span = std::span{reinterpret_cast<std::byte*>(&data), sizeof(data)};
			return make_function_prototype(data_span, length);
		}
};

struct EXPORT accept_vm_value : accept_vm_primitive {
	public:
		explicit accept_vm_value(const runtime_lock& lock) : accept_vm_primitive{lock} {}
		using accept_vm_primitive::operator();

		[[nodiscard]] auto lock() const -> const runtime_lock& {
			return reinterpret_cast<const runtime_lock&>(accept_vm_primitive::lock());
		}
};

} // namespace isolated_vm

namespace js {
using namespace isolated_vm;

// Primitive acceptor
template <std::convertible_to<value_tag> Tag>
	requires std::is_base_of_v<primitive_tag, Tag>
struct accept<void, value_of<Tag>> : accept_vm_primitive {
		using accept_vm_primitive::accept_vm_primitive;
};

// Template / prototype acceptor
template <class Tag>
	requires std::is_base_of_v<prototype_tag, Tag>
struct accept<void, value_of<Tag>> : accept_vm_prototype {
		using accept_vm_prototype::accept_vm_prototype;
};

// Value acceptor
template <std::convertible_to<value_tag> Tag>
struct accept<void, value_of<Tag>> : accept_vm_value {
		using accept_vm_value::accept_vm_value;
};

} // namespace js

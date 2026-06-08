export module isolated_vm:transfer.visit;
import :value;
import auto_js;
import std;
import util;

namespace isolated_vm {
using namespace js;

constexpr auto primitive_typeofs = std::array{
	value_typeof::undefined,
	value_typeof::null,
	value_typeof::boolean,
	value_typeof::number,
	value_typeof::number_i32,
	value_typeof::string,
	value_typeof::string_latin1,
	value_typeof::symbol,
	value_typeof::bigint,
	value_typeof::bigint_i64,
};

constexpr auto data_block_typeofs = std::array{
	value_typeof::array_buffer,
	value_typeof::shared_array_buffer,
};

constexpr auto array_buffer_view_typeofs = std::array{
	value_typeof::bigint64_array,
	value_typeof::biguint64_array,
	value_typeof::data_view,
	value_typeof::float16_array,
	value_typeof::float32_array,
	value_typeof::float64_array,
	value_typeof::int16_array,
	value_typeof::int32_array,
	value_typeof::int8_array,
	value_typeof::uint16_array,
	value_typeof::uint32_array,
	value_typeof::uint8_array,
	value_typeof::uint8_clamped_array,
};

template <class Visit>
struct visit_vm_property_key {
	public:
		explicit visit_vm_property_key(Visit& visit) : visit_{visit} {}

		template <class Accept>
		auto operator()(value_of<primitive_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto accept_as = [ & ]<class Tag>(Tag tag) -> auto {
				auto value = bound_value{lock(), value_of<Tag>::from(subject)};
				return accept(tag, *this, value);
			};
			if (subject.is_string()) {
				if (value_of<string_tag>::from(subject).is_latin1()) {
					return accept_as(string_tag_of<char>{});
				} else {
					return accept_as(string_tag_of<char16_t>{});
				}
			} else if (subject.is_number()) {
				return accept_as(number_tag_of<std::int32_t>{});
			} else {
				return accept_as(symbol_tag{});
			}
		}

		[[nodiscard]] auto lock() const -> auto& { return visit_.get().lock(); }

	private:
		std::reference_wrapper<Visit> visit_;
};

template <auto Key>
struct visit_vm_key_literal {
		explicit constexpr visit_vm_key_literal(auto* /*transfer*/) {}

		auto operator()(const auto& /*anything*/, const auto& accept_or_visit) -> value_of<name_tag> {
			if (!local_key_) {
				constexpr auto key = util::make_consteval_string_view(Key);
				local_key_ = transfer_in<value_of<name_tag>>(key, accept_or_visit.lock());
			}
			return local_key_;
		}

	private:
		value_of<name_tag> local_key_;
};

struct visit_vm_value {
	public:
		explicit visit_vm_value(const runtime_lock& lock) : lock_{lock} {}

		template <class Accept>
		auto operator()(value_of<> subject, const Accept& accept) -> accept_target_t<Accept> {
			// TODO: reference map
			return util::template_traverse(
				accept_tags_of_v<Accept>,
				util::overloaded{
					// Fast paths
					[ & ](undefined_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_undefined() ? accept_as(undefined_tag{}, subject, accept) : next();
					},
					[ & ](null_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_null() ? accept_as(null_tag{}, subject, accept) : next();
					},
					[ & ](boolean_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_boolean() ? accept_as(boolean_tag{}, subject, accept) : next();
					},
					[ & ](number_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_number() ? accept_as(number_tag{}, subject, accept) : next();
					},
					[ & ](string_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_string() ? accept_as(string_tag{}, subject, accept) : next();
					},
					[ & ](bigint_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_bigint() ? accept_as(bigint_tag{}, subject, accept) : next();
					},
					[ & ](date_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_date() ? accept_as(date_tag{}, subject, accept) : next();
					},
					[ & ](list_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_array() ? accept_as(list_tag{}, subject, accept) : next();
					},
					[ & ](function_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return subject.is_function() ? accept_as(function_tag{}, subject, accept) : next();
					},

					// Unknown tag
					[](auto /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },

					// Slow path
					[ & ]() -> accept_target_t<Accept> {
						auto type = subject.inspect();
						const auto [... primitive_types ] = primitive_typeofs;
						if ((... || (type == primitive_types))) {
							return inspected(type, value_of<primitive_tag>::from(subject), accept);
						} else {
							return inspected(type, value_of<object_tag>::from(subject), accept);
						}
					},
				}
			);
		}

		template <class Accept>
		auto operator()(value_of<primitive_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return inspected(subject.inspect(), subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<number_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (subject.inspect()) {
				case value_typeof::number: return accept_as(number_tag{}, subject, accept);
				case value_typeof::number_i32: return accept_as(number_tag_of<std::int32_t>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		auto operator()(value_of<string_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (subject.inspect()) {
				case value_typeof::string: return accept_as(string_tag_of<char16_t>{}, subject, accept);
				case value_typeof::string_latin1: return accept_as(string_tag_of<char>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		auto operator()(value_of<bigint_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (subject.inspect()) {
				case value_typeof::bigint: return accept_as(bigint_tag{}, subject, accept);
				case value_typeof::bigint_i64: return accept_as(bigint_tag_of<std::int64_t>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		auto operator()(value_of<object_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return inspected(subject.inspect(), subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<data_block_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return inspected(subject.inspect(), subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<array_buffer_view_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return inspected(subject.inspect(), subject, accept);
		}

		[[nodiscard]] auto lock() const -> const runtime_lock& { return lock_.get(); }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		template <class Accept>
		constexpr auto inspected(value_typeof type_of, value_of<primitive_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (type_of) {
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
				default: std::unreachable();
			}
		}

		template <class Accept>
		constexpr auto inspected(value_typeof type_of, value_of<object_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			const auto [... data_block_types ] = data_block_typeofs;
			const auto [... array_buffer_view_types ] = array_buffer_view_typeofs;
			if ((... || (type_of == data_block_types))) {
				return inspected(type_of, value_of<data_block_tag>::from(subject), accept);
			} else if ((... || (type_of == array_buffer_view_types))) {
				return inspected(type_of, value_of<array_buffer_view_tag>::from(subject), accept);
			} else {
				switch (type_of) {
					case value_typeof::external: return accept_as(external_tag{}, subject, accept);
					case value_typeof::date: return accept_as(date_tag{}, subject, accept);
					case value_typeof::promise: return accept_as(promise_tag{}, subject, accept);
					case value_typeof::function: return accept_as(function_tag{}, subject, accept);
					case value_typeof::array:
						{
							auto visit_entry = visit_entry_pair<visit_vm_property_key<visit_vm_value>, visit_vm_value&>{*this};
							return accept(list_tag{}, visit_entry, bound_value{lock_, value_of<record_tag>::from(subject)});
						}
					case value_typeof::dictionary:
						{
							auto visit_entry = visit_entry_pair<visit_vm_property_key<visit_vm_value>, visit_vm_value&>{*this};
							return accept(dictionary_tag{}, visit_entry, bound_value{lock_, value_of<record_tag>::from(subject)});
						}
					default: std::unreachable();
				}
			}
		}

		template <class Accept>
		constexpr auto inspected(value_typeof type_of, value_of<data_block_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (type_of) {
				case value_typeof::array_buffer: return accept_as(array_buffer_tag{}, subject, accept);
				case value_typeof::shared_array_buffer: return accept_as(shared_array_buffer_tag{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept>
		constexpr auto inspected(value_typeof type_of, value_of<array_buffer_view_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			switch (type_of) {
				case value_typeof::bigint64_array: return accept_as(typed_array_tag_of<std::int64_t>{}, subject, accept);
				case value_typeof::biguint64_array: return accept_as(typed_array_tag_of<std::uint64_t>{}, subject, accept);
				case value_typeof::data_view: return accept_as(data_view_tag{}, subject, accept);
				case value_typeof::float16_array: return accept_as(typed_array_tag_of<js::float16_t>{}, subject, accept);
				case value_typeof::float32_array: return accept_as(typed_array_tag_of<float>{}, subject, accept);
				case value_typeof::float64_array: return accept_as(typed_array_tag_of<double>{}, subject, accept);
				case value_typeof::int16_array: return accept_as(typed_array_tag_of<std::int16_t>{}, subject, accept);
				case value_typeof::int32_array: return accept_as(typed_array_tag_of<std::int32_t>{}, subject, accept);
				case value_typeof::int8_array: return accept_as(typed_array_tag_of<std::int8_t>{}, subject, accept);
				case value_typeof::uint16_array: return accept_as(typed_array_tag_of<std::uint16_t>{}, subject, accept);
				case value_typeof::uint32_array: return accept_as(typed_array_tag_of<std::uint32_t>{}, subject, accept);
				case value_typeof::uint8_array: return accept_as(typed_array_tag_of<std::uint8_t>{}, subject, accept);
				case value_typeof::uint8_clamped_array: return accept_as(typed_array_tag_of<js::uint8_clamped_t>{}, subject, accept);
				default: std::unreachable();
			}
		}

		template <class Accept, class Tag>
		auto accept_as(Tag tag, value_of<> subject, const Accept& accept) -> accept_target_t<Accept> {
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

template <auto Key>
struct visit_key_literal<Key, value_of<record_tag>> : visit_vm_key_literal<Key> {
		using visit_vm_key_literal<Key>::visit_vm_key_literal;
};

} // namespace js

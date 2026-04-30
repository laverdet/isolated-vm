export module napi_js:accept;
import :api;
import :container;
import :environment_fwd;
import :value;
import std;
import util;

namespace js::napi {

// napi reference acceptor
struct reaccept_napi_value {
		using reference_type = napi_value;

		constexpr auto operator()(std::type_identity<napi_value> /*type*/, napi_value value) const -> napi_value {
			return value;
		}

		template <class Tag>
		constexpr auto operator()(std::type_identity<value_of<Tag>> /*type*/, napi_value value) const -> value_of<Tag> {
			return value_of<Tag>::from(value);
		}
};

// Napi acceptor which does not need specialized environment type
struct accept_basic_napi_value {
	public:
		explicit accept_basic_napi_value(auto* /*transfer*/, auto& env) : env_{env} {}

		// Declare reference provider
		using accept_reference_type = reaccept_napi_value;

		// undefined & null
		auto operator()(undefined_tag tag, visit_holder visit, std::monostate subject) const -> value_of<undefined_tag>;
		auto operator()(undefined_tag tag, visit_holder visit, const auto& /*subject*/) const -> value_of<undefined_tag> {
			return (*this)(tag, visit, std::monostate{});
		}

		auto operator()(null_tag tag, visit_holder visit, std::nullptr_t subject) const -> value_of<null_tag>;
		auto operator()(null_tag tag, visit_holder visit, const auto& /*null*/) const -> value_of<null_tag> {
			return (*this)(tag, visit, nullptr);
		}

		// boolean
		auto operator()(boolean_tag tag, visit_holder visit, bool subject) const -> value_of<boolean_tag>;

		// number
		auto operator()(number_tag_of<double> tag, visit_holder visit, double subject) const -> value_of<number_tag_of<double>>;
		auto operator()(number_tag_of<std::int32_t> tag, visit_holder visit, std::int32_t) const -> value_of<number_tag_of<std::int32_t>>;
		auto operator()(number_tag_of<std::uint32_t> tag, visit_holder visit, std::uint32_t) const -> value_of<number_tag_of<std::uint32_t>>;

		auto operator()(number_tag /*tag*/, visit_holder visit, auto&& subject) const -> value_of<number_tag> {
			return (*this)(number_tag_of<double>{}, visit, double{std::forward<decltype(subject)>(subject)});
		}

		template <class Type>
		auto operator()(number_tag_of<Type> tag, visit_holder visit, auto&& subject) const -> value_of<number_tag_of<Type>> {
			return (*this)(tag, visit, Type{std::forward<decltype(subject)>(subject)});
		}

		// string
		auto operator()(string_tag_of<char> tag, visit_holder visit, std::string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char>>>;
		auto operator()(string_tag_of<char8_t> tag, visit_holder visit, std::u8string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char8_t>>>;
		auto operator()(string_tag_of<char16_t> tag, visit_holder visit, std::u16string_view subject) const
			-> js::referenceable_value<value_of<string_tag_of<char16_t>>>;

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

		// date
		auto operator()(date_tag tag, visit_holder /*visit*/, js_clock::time_point subject) const
			-> js::referenceable_value<value_of<date_tag>>;

		// error
		auto operator()(error_tag /*tag*/, visit_holder visit, const js::error_value& subject) const
			-> js::referenceable_value<value_of<error_tag>>;

		auto operator()(error_tag tag, visit_holder visit, const auto& subject) const
			-> js::referenceable_value<value_of<error_tag>> {
			return (*this)(tag, visit, js::error_value{subject});
		}

		// data blocks (array buffer, shared array buffer)
		auto operator()(array_buffer_tag tag, visit_holder visit, const js::array_buffer& subject) const
			-> js::referenceable_value<value_of<array_buffer_tag>>;
		auto operator()(shared_array_buffer_tag tag, visit_holder visit, js::shared_array_buffer&& subject) const
			-> js::referenceable_value<value_of<shared_array_buffer_tag>>;
		auto operator()(shared_array_buffer_tag tag, visit_holder visit, const js::shared_array_buffer& subject) const
			-> js::referenceable_value<value_of<shared_array_buffer_tag>>;

		// typed arrays & data view
		template <std::convertible_to<array_buffer_view_tag> Tag>
		auto operator()(this const auto& self, Tag /*tag*/, auto& visit, auto&& subject)
			-> js::referenceable_value<value_of<Tag>> {
			auto byte_offset = subject.byte_offset();
			auto length = subject.size();
			// TODO: We accept the nested `buffer` property as a `data_block` which means `make` needs to
			// invoke `is_arraybuffer` on it again even though we knew what it was a moment ago.
			auto buffer = value_of<data_block_tag>::from(visit(std::forward<decltype(subject)>(subject).buffer(), self));
			return js::referenceable_value{value_of<Tag>::make(self.environment(), buffer, byte_offset, length)};
		}

		// extras
		[[nodiscard]] auto environment() const -> environment& { return env_; }
		explicit operator napi_env() const { return napi_env{env_.get()}; }

	private:
		std::reference_wrapper<napi::environment> env_;
};

// Generic napi acceptor which accepts all value types.
template <class Environment>
struct accept_napi_value;

template <class Meta>
using accept_napi_value_with = accept_napi_value<typename Meta::accept_context_type>;

template <class Environment>
struct accept_napi_value : accept_basic_napi_value {
	public:
		explicit accept_napi_value(auto* transfer, auto& env) :
				accept_basic_napi_value{transfer, env} {}
		using accept_basic_napi_value::operator();

		// function
		template <class Callback>
		auto operator()(function_tag /*tag*/, visit_holder /*visit*/, js::free_function<Callback> subject) const -> value_of<function_tag> {
			return value_of<function_tag>::make(environment(), std::forward<decltype(subject)>(subject));
		}

		// vectors
		auto operator()(this const auto& self, vector_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<vector_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, subject.size())),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<vector_tag> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					int ii = 0;
					auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
					for (auto&& subject : util::forward_range(std::forward<decltype(range)>(range))) {
						auto* element = napi_value{visit(std::forward<decltype(subject)>(subject), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, ii++, element);
					}
				},
			};
		}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<vector_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<vector_tag>::from(napi::invoke(napi_create_array_with_length, napi_env{self}, Size)),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<vector_tag> array, auto& self, auto& visit, auto /*&&*/ tuple) -> void {
					const auto [... indices ] = util::sequence<Size>;
					(..., [ & ]() -> void {
						// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
						// reference members one at a time.
						auto* element = napi_value{visit(indices, std::forward<decltype(tuple)>(tuple), self)};
						napi::invoke0(napi_set_element, napi_env{self}, napi_value{array}, indices, element);
					}());
				}
			};
		}

		// arrays & dictionaries
		auto operator()(this const auto& self, list_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<list_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<list_tag>::from(napi::invoke(napi_create_array, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<list_tag> array, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					self.accept_entry_pair_vector(visit, array, std::forward<decltype(subject)>(subject));
				}
			};
		}

		auto operator()(this const auto& self, dictionary_tag /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<dictionary_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<dictionary_tag> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					self.accept_entry_pair_vector(visit, object, std::forward<decltype(subject)>(subject));
				}
			};
		}

		auto accept_entry_pair_vector(this const auto& self, auto& visit, value_of<object_tag> target, auto&& subject) -> void {
			std::vector<napi_property_descriptor> properties;
			auto&& range = util::into_range(std::forward<decltype(subject)>(subject));
			properties.reserve(std::size(range));
			for (auto&& [ key, value ] : util::forward_range(std::forward<decltype(range)>(range))) {
				auto* name = napi_value{visit.first(std::forward<decltype(key)>(key), self)};
				properties.emplace_back(napi_property_descriptor{
					.utf8name{},
					// TODO: `napi_define_properties` only works with string keys but the nested acceptor will
					// accept indexed properties as numeric napi values. `visit.first` should be invoked
					// against something that maybe resolves to `std::variant<int32_t, value_of<name_tag>>`.
					.name = napi::invoke(napi_coerce_to_string, napi_env{self}, name),

					.method{},
					.getter{},
					.setter{},
					.value = napi_value{visit.second(std::forward<decltype(value)>(value), self)},

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				});
			}
			napi::invoke0(napi_define_properties, napi_env{self}, napi_value{target}, properties.size(), properties.data());
		}

		// structs
		template <std::size_t Size>
		auto operator()(this const auto& self, struct_tag<Size> /*tag*/, auto& visit, auto&& subject)
			-> js::deferred_receiver<value_of<dictionary_tag>, decltype(self), decltype(visit), decltype(subject)> {
			return {
				value_of<dictionary_tag>::from(napi::invoke(napi_create_object, napi_env{self})),
				std::forward_as_tuple(self, visit, std::forward<decltype(subject)>(subject)),
				[](value_of<dictionary_tag> object, auto& self, auto& visit, auto /*&&*/ subject) -> void {
					self.template accept_entry_pair_struct<Size>(visit, object, std::forward<decltype(subject)>(subject));
				}
			};
		}

		template <std::size_t Size>
		auto accept_entry_pair_struct(this const auto& self, auto& visit, value_of<object_tag> target, auto&& subject) -> void {
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
			std::array<napi_property_descriptor, Size> properties;
			const auto [... indices ] = util::sequence<Size>;
			(..., [ & ]() -> void {
				// nb: This is forwarded to *each* visitor. The visitor should be aware and only lvalue
				// reference members one at a time.
				auto accept_entry = accept_entry_pair<decltype(self), decltype(self)>{self};
				auto entry = visit(std::integral_constant<std::size_t, indices>{}, std::forward<decltype(subject)>(subject), accept_entry);
				// NOLINTNEXTLINE(modernize-type-traits)
				properties.at(indices) = {
					.utf8name{},
					.name = entry.first,

					.method{},
					.getter{},
					.setter{},
					.value = entry.second,

					// NOLINTNEXTLINE(hicpp-signed-bitwise)
					.attributes = static_cast<napi_property_attributes>(napi_writable | napi_enumerable | napi_configurable),
					.data{},
				};
			}());
			napi::invoke0(napi_define_properties, napi_env{self}, napi_value{target}, properties.size(), properties.data());
		}

		// extras
		[[nodiscard]] auto environment() const -> Environment& {
			return static_cast<Environment&>(accept_basic_napi_value::environment());
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// `value_of<object_tag>{}.assign(...)` implementation
struct object_assign_delegate {
	public:
		explicit object_assign_delegate(value_of<object_tag> object) : object_{object} {}
		auto operator*() const { return object_; }

	private:
		value_of<object_tag> object_;
};

auto value_for_object::assign(this value_of<object_tag> self, auto_environment auto& env, auto source) -> void {
	js::transfer_in<object_assign_delegate>(std::move(source), env, object_assign_delegate{self});
}

} // namespace js::napi

namespace js {

// Plain napi_value acceptor
template <>
struct accept_property_subject<napi_value> : std::type_identity<napi_value> {};

template <class Meta>
struct accept<Meta, napi_value> : napi::accept_napi_value_with<Meta> {
		using accept_type = napi::accept_napi_value_with<Meta>;
		using accept_type::accept_type;
};

// Tagged `value_of<T>` acceptor
template <>
struct accept_property_subject<napi::value_of<value_tag>> : std::type_identity<napi_value> {};

template <class Meta, class Tag>
struct accept<Meta, napi::value_of<Tag>> : napi::accept_napi_value_with<Meta> {
		using accept_type = napi::accept_napi_value_with<Meta>;
		using accept_type::accept_type;
};

// Forwarded `value_of<T>` acceptor
template <class Tag>
struct accept<void, js::forward<napi::value_of<Tag>, Tag>> {
		auto operator()(Tag /*tag*/, visit_holder /*visit*/, napi::value_of<Tag> value) const -> js::forward<napi::value_of<Tag>, Tag> {
			return js::forward{napi::value_of<Tag>{value}, Tag{}};
		}

		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }
};

// Object key lookup via napi
template <class Meta, auto Key, class Type>
struct accept_property_value<Meta, Key, Type, napi_value> {
	public:
		explicit constexpr accept_property_value(auto* transfer) :
				first{transfer},
				second{transfer} {}

		auto operator()(dictionary_tag /*tag*/, auto& visit, const auto& object) const -> Type {
			if (auto local = first(std::type_identity<void>{}, visit.first); object.has(local)) {
				return visit.second(object.get(local), second);
			} else {
				return second(undefined_in_tag{}, visit.second, std::monostate{});
			}
		}

	private:
		mutable visit_key_literal<Key, napi_value> first;
		accept_value<Meta, Type> second;
};

template <class Meta>
struct accept<Meta, napi::object_assign_delegate> {
	public:
		accept(auto* transfer, auto& env, napi::object_assign_delegate object) :
				accept_{transfer, env},
				object_{object} {}

		template <std::size_t Size>
		auto operator()(this const auto& self, tuple_tag<Size> /*tag*/, auto& visit, auto&& subject) -> napi::object_assign_delegate {
			self.accept_.template accept_entry_pair_struct<Size>(visit, *self.object_, std::forward<decltype(subject)>(subject));
			return self.object_;
		}

		consteval static auto types(auto recursive) { return accept<Meta, napi_value>::types(recursive); }

	private:
		accept_value<Meta, napi_value> accept_;
		napi::object_assign_delegate object_;
};

} // namespace js

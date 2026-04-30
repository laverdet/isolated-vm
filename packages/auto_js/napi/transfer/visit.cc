export module napi_js:visit;
import :api;
import :container;
import :environment_fwd;
import :utility;
import :value;
import std;
import util;
import v8;

namespace js {
using namespace napi;

// Instantiated in the acceptor that corresponds to a napi visitor
struct napi_reference_map_type {
		template <class Type>
		using type = napi::value_map<Type>;
};

// Visitor which may visit only property names. `symbol` should probably be rethought since they are
// not cloneable.
template <class Visit>
struct visit_napi_property_name {
	public:
		explicit visit_napi_property_name(Visit& visit) : visit_{visit} {}

		template <class Accept>
		auto operator()(napi_value subject, const Accept& accept) -> accept_target_t<Accept> {
			return visit_.get().lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				switch (napi::invoke(napi_typeof, napi_env{visit_.get()}, subject)) {
					case napi_number:
						return visit_(value_of<number_tag_of<std::int32_t>>::from(subject), accept);
					case napi_string:
						// TODO: This looks up in the reference map twice. This should actually use
						// `make_property_name`.
						return visit_(value_of<string_tag>::from(subject), accept);
					case napi_symbol:
						return visit_(value_of<symbol_tag>::from(subject), accept);
					default:
						std::unreachable();
				}
			});
		}

		[[nodiscard]] auto environment() const -> auto& { return visit_.get().environment(); }

	private:
		std::reference_wrapper<Visit> visit_;
};

// Base napi visitor implementing all functionality. Napi doesn't give us granular information like
// "is this a latin1 string" and all checks must be made at once. So it's structured it great deal
// differently than the v8 visitor.
template <auto_environment Environment, class Ref>
struct visit_napi_value;

template <class Meta>
using visit_napi_value_with = visit_napi_value<
	typename Meta::visit_context_type,
	typename Meta::accept_reference_type>;

template <auto_environment Environment, class Reference>
struct visit_napi_value
		: napi::environment_scope<Environment>,
			reference_map_t<Reference, napi_reference_map_type> {
	public:
		using reference_map_t<Reference, napi_reference_map_type>::lookup_or_visit;

		visit_napi_value(auto* /*transfer*/, Environment& env) :
				napi::environment_scope<Environment>{env},
				reference_map_t<Reference, napi_reference_map_type>{env} {}

		// If the private `immediate` operation is defined: this public operation will first
		// perform a reference map lookup, then delegate to the private operation if not found.
		template <class Tag, class Accept>
		auto operator()(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept>
			requires requires { immediate(subject, accept); } {
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				return immediate(subject, accept);
			});
		}

		// Visit operations for non-refable types.
		template <class Accept>
		auto operator()(value_of<null_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			null_ = subject;
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<undefined_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			undefined_ = subject;
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<boolean_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<number_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (std::bit_cast<v8::Local<v8::Number>>(subject)->IsInt32()) {
				return accept_tagged(value_of<number_tag_of<std::int32_t>>::from(subject), accept);
			} else {
				return accept_tagged(value_of<number_tag_of<double>>::from(subject), accept);
			}
		}

		template <class Accept, class Type>
		auto operator()(value_of<number_tag_of<Type>> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(value_of<number_tag_of<Type>>::from(subject), accept);
		}

		template <class Accept>
		auto operator()(value_of<symbol_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// General purpose visit operation which actually performs the type check
		template <class Accept>
		auto operator()(napi_value subject, const Accept& accept) -> accept_target_t<Accept> {

			// Check the reference map, and lookup type via napi
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				auto type_of = napi::invoke(napi_typeof, napi_env{*this}, subject);
				switch (type_of) {
					case napi_undefined:
						return (*this)(value_of<undefined_tag>::from(subject), accept);
					case napi_null:
						return (*this)(value_of<null_tag>::from(subject), accept);
					case napi_boolean:
						return (*this)(value_of<boolean_tag>::from(subject), accept);
					case napi_number:
						return (*this)(value_of<number_tag>::from(subject), accept);
					case napi_string:
						return immediate(value_of<string_tag>::from(subject), accept);
					case napi_symbol:
						return (*this)(value_of<symbol_tag>::from(subject), accept);
					case napi_object:
						{
							if (napi::invoke(napi_is_array, napi_env{*this}, subject)) {
								return immediate(value_of<list_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_date, napi_env{*this}, subject)) {
								return immediate(value_of<date_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_typedarray, napi_env{*this}, subject)) {
								return immediate(value_of<typed_array_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_dataview, napi_env{*this}, subject)) {
								return immediate(value_of<data_view_tag>::from(subject), accept);
							} else if (std::bit_cast<v8::Local<v8::Object>>(subject)->IsSharedArrayBuffer()) {
								return immediate(value_of<shared_array_buffer_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_arraybuffer, napi_env{*this}, subject)) {
								return immediate(value_of<array_buffer_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_promise, napi_env{*this}, subject)) {
								return immediate(value_of<promise_tag>::from(subject), accept);
							} else {
								return immediate(value_of<dictionary_tag>::from(subject), accept);
							}
						}
					case napi_function:
						return immediate(value_of<function_tag>::from(subject), accept);
					case napi_external:
						return immediate(value_of<external_tag>::from(subject), accept);
					case napi_bigint:
						return immediate(value_of<bigint_tag>::from(subject), accept);
				}
				std::unreachable();
			});
		}

		// no required types
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		// Private visit operations for refable types
		template <class Accept>
		auto immediate(value_of<string_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(value_of<string_tag_of<char16_t>>::from(subject), accept);
		}

		template <class Accept>
		auto immediate(value_of<date_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<promise_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<data_block_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (napi::invoke(napi_is_arraybuffer, napi_env{*this}, subject)) {
				return immediate(value_of<array_buffer_tag>::from(subject), accept);
			} else {
				return immediate(value_of<shared_array_buffer_tag>::from(subject), accept);
			}
		}

		template <class Accept>
		auto immediate(value_of<array_buffer_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<shared_array_buffer_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<typed_array_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto bound_subject_variant = bound_value_for_typed_array::make_bound(this->environment(), subject);
			if (bound_subject_variant.index() == std::variant_npos) {
				std::unreachable();
			} else {
				return std::visit(
					[ & ]<class Tag>(bound_value<Tag> value) -> accept_target_t<Accept> {
						return accept(Tag{}, *this, value);
					},
					bound_subject_variant
				);
			}
		}

		template <class Accept>
		auto immediate(value_of<data_view_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<external_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<function_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value_of<bigint_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(value_of<bigint_tag_of<bigint>>::from(subject), accept);
		}

		template <class Accept>
		auto immediate(value_of<list_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto target = napi::bound_value{napi_env{*this}, value_of<list_tag>::from(subject)};
			auto visit_entry = visit_entry_pair<visit_napi_property_name<visit_napi_value>, visit_napi_value&>{*this};
			return accept(list_tag{}, visit_entry, target);
		}

		template <class Accept>
		auto immediate(value_of<dictionary_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto target = napi::bound_value{napi_env{*this}, value_of<dictionary_tag>::from(subject)};
			auto visit_entry = visit_entry_pair<visit_napi_property_name<visit_napi_value>, visit_napi_value&>{*this};
			return accept(dictionary_tag{}, visit_entry, target);
		}

		// Convenience function which wraps in `napi::bound_value` and invokes `accept`.
		template <class Tag, class Accept>
		[[nodiscard]] auto accept_tagged(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(Tag{}, *this, napi::bound_value{this->environment(), subject});
		}

		napi_value undefined_{napi::null_value_handle};
		napi_value null_{napi::null_value_handle};
};

// Napi value visitor entry
template <class Meta>
struct visit<Meta, napi_value> : visit_napi_value_with<Meta> {
		using visit_napi_value_with<Meta>::visit_napi_value_with;
};

template <class Meta, class Tag>
struct visit<Meta, value_of<Tag>> : visit_napi_value_with<Meta> {
		using visit_napi_value_with<Meta>::visit_napi_value_with;
};

// Object key maker via napi
template <auto Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		explicit constexpr visit_key_literal(auto* /*transfer*/) {}

		auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept_or_visit) -> napi_value {
			if (local_key_ == napi_value{}) {
				auto& environment = accept_or_visit.environment();
				auto storage = environment.string_table_storage(Key);
				if (storage) {
					auto& reference = *storage;
					if (reference) {
						local_key_ = reference.get(environment);
					} else {
						auto value = value_of<string_tag>::make_property_name(environment, util::make_consteval_string_view(Key));
						reference.reset(environment, value);
						local_key_ = napi_value{value};
					}
				} else {
					return value_of<string_tag>::make_property_name(environment, util::make_consteval_string_view(Key));
				}
			}
			return local_key_;
		}

	private:
		napi_value local_key_{};
};

} // namespace js

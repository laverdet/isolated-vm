module;
#include <functional>
#include <string_view>
#include <utility>
export module napi_js:visit;
import :api;
import :bound_value;
import :container;
import :dictionary;
import :environment;
import :external;
import :function;
import :primitive;
import :value;
import isolated_js;
import ivm.utility;

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
		explicit visit_napi_property_name(const Visit& visit) : visit_{visit} {}

		template <class Accept>
		auto operator()(napi_value subject, Accept& accept) const -> accept_target_t<Accept> {
			return visit_.get().lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
				switch (napi::invoke(napi_typeof, napi_env{visit_.get()}, subject)) {
					case napi_number:
						return visit_(napi::value<number_tag>::from(subject), accept);
					case napi_string:
						// TODO: This looks up in the reference map twice. This should actually use
						// `make_property_name`.
						return visit_(napi::value<string_tag>::from(subject), accept);
					case napi_symbol:
						return visit_(napi::value<symbol_tag>::from(subject), accept);
					default:
						std::unreachable();
				}
			});
		}

		[[nodiscard]] auto environment() const -> auto& { return visit_.get().environment(); }

	private:
		std::reference_wrapper<const Visit> visit_;
};

// Base napi visitor implementing all functionality. Napi doesn't give us granular information like
// "is this a latin1 string" and all checks must be made at once. So it's structured it great deal
// differently than the v8 visitor.
template <class Environment, class Target>
struct visit_napi_value;

template <class Meta>
using visit_napi_value_with = visit_napi_value<
	typename Meta::visit_context_type,
	typename Meta::accept_reference_type>;

template <class Environment, class Target>
struct visit_napi_value
		: napi::environment_scope<Environment>,
			reference_map_t<Target, napi_reference_map_type> {
	public:
		using reference_map_type = reference_map_t<Target, napi_reference_map_type>;
		using reference_map_type::lookup_or_visit;
		using reference_map_type::try_emplace;

		visit_napi_value(auto* /*transfer*/, Environment& env) :
				napi::environment_scope<Environment>{env},
				reference_map_type{env},
				equal_{env} {}

		// If the private `immediate` operation is defined: this public operation will first
		// perform a reference map lookup, then delegate to the private operation if not found.
		template <auto_tag Tag, class Accept>
		auto operator()(this const auto& self, value<Tag> subject, Accept& accept) -> accept_target_t<Accept>
			requires requires { self.immediate(subject, accept); } {
			return self.lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
				return self.immediate(subject, accept);
			});
		}

		// Visit operations for non-refable types.
		template <class Accept>
		auto operator()(value<null_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			null_ = subject;
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value<undefined_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			undefined_ = subject;
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value<boolean_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto operator()(value<number_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(napi::value<number_tag_of<double>>::from(subject), accept);
		}

		template <class Accept>
		auto operator()(value<symbol_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// General purpose visit operation which actually performs the type check
		template <class Accept>
		auto operator()(napi_value subject, Accept& accept) const -> accept_target_t<Accept> {
			// Check known address values before the map lookup
			if (equal_(subject, undefined_)) {
				return (*this)(napi::value<undefined_tag>::from(subject), accept);
			} else if (equal_(subject, null_)) {
				return (*this)(napi::value<null_tag>::from(subject), accept);
			}

			// Check the reference map, and lookup type via napi
			return lookup_or_visit(accept, subject, [ & ]() -> accept_target_t<Accept> {
				auto typeof = napi::invoke(napi_typeof, napi_env{*this}, subject);
				switch (typeof) {
					case napi_undefined:
						return (*this)(napi::value<undefined_tag>::from(subject), accept);
					case napi_null:
						return (*this)(napi::value<null_tag>::from(subject), accept);
					case napi_boolean:
						return (*this)(napi::value<boolean_tag>::from(subject), accept);
					case napi_number:
						return (*this)(napi::value<number_tag>::from(subject), accept);
					case napi_string:
						return immediate(napi::value<string_tag>::from(subject), accept);
					case napi_symbol:
						return (*this)(napi::value<symbol_tag>::from(subject), accept);
					case napi_object:
						{
							if (napi::invoke(napi_is_array, napi_env{*this}, subject)) {
								return immediate(napi::value<list_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_date, napi_env{*this}, subject)) {
								return immediate(napi::value<date_tag>::from(subject), accept);
							} else if (napi::invoke(napi_is_promise, napi_env{*this}, subject)) {
								return immediate(napi::value<promise_tag>::from(subject), accept);
							}
							return immediate(napi::value<object_tag>::from(subject), accept);
						}
					case napi_function:
						return immediate(napi::value<function_tag>::from(subject), accept);
					case napi_external:
						return immediate(napi::value<external_tag>::from(subject), accept);
					case napi_bigint:
						return immediate(napi::value<bigint_tag>::from(subject), accept);
				}
			});
		}

	private:
		// Private visit operations for refable types
		template <class Accept>
		auto immediate(value<string_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(napi::value<string_tag_of<char16_t>>::from(subject), accept);
		}

		template <class Accept>
		auto immediate(value<date_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value<promise_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value<external_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value<function_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		template <class Accept>
		auto immediate(value<bigint_tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return accept_tagged(napi::value<bigint_tag_of<bigint>>::from(subject), accept);
		}

		template <class Visit, class Accept>
		auto immediate(this const Visit& self, value<list_tag> subject, Accept& accept) -> accept_target_t<Accept> {
			// nb: It is intentional that `dictionary_tag` is bound. It handles sparse arrays.
			auto target = napi::bound_value{napi_env{self}, napi::value<dictionary_tag>::from(subject)};
			auto visit_entry = std::pair<visit_napi_property_name<Visit>, const Visit&>{self, self};
			return self.try_emplace(accept, list_tag{}, visit_entry, target);
		}

		template <class Visit, class Accept>
		auto immediate(this const Visit& self, value<object_tag> subject, Accept& accept) -> accept_target_t<Accept> {
			auto target = napi::bound_value{napi_env{self}, napi::value<dictionary_tag>::from(subject)};
			auto visit_entry = std::pair<visit_napi_property_name<Visit>, const Visit&>{self, self};
			return self.try_emplace(accept, dictionary_tag{}, visit_entry, target);
		}

		// Convenience function which wraps in `napi::bound_value` and invokes `accept`.
		template <auto_tag Tag, class Accept>
		auto accept_tagged(value<Tag> subject, Accept& accept) const -> accept_target_t<Accept> {
			return try_emplace(accept, Tag{}, *this, napi::bound_value{napi_env{*this}, subject});
		}

		napi::address_equal equal_;
		mutable napi_value undefined_{napi::null_value_handle};
		mutable napi_value null_{napi::null_value_handle};
};

// Napi value visitor entry
template <class Meta>
struct visit<Meta, napi_value> : visit_napi_value_with<Meta> {
		using visit_napi_value_with<Meta>::visit_napi_value_with;
};

template <class Meta>
struct visit<Meta, value<value_tag>> : visit_napi_value_with<Meta> {
		using visit_napi_value_with<Meta>::visit_napi_value_with;
};

// Object key maker via napi
template <util::string_literal Key>
struct visit_key_literal<Key, napi_value> : util::non_moveable {
	public:
		template <class Accept>
		[[nodiscard]] auto get_local(const Accept& accept_or_visit) const -> napi_value {
			if (local_key_ == napi_value{}) {
				auto& environment = accept_or_visit.environment();
				auto& storage = environment.global_storage(util::value_constant<Key>{});
				if (storage) {
					local_key_ = storage.get(environment);
				} else {
					auto value = napi::value<string_tag>::make_property_name(environment, std::string_view{Key});
					storage.reset(environment, value);
					local_key_ = napi_value{value};
				}
			}
			return local_key_;
		}

		template <class Accept>
		auto operator()(const auto& /*could_be_literally_anything*/, Accept& accept) const -> accept_target_t<Accept> {
			return accept(string_tag{}, *this, get_local(accept));
		}

	private:
		mutable napi_value local_key_{};
};

} // namespace js

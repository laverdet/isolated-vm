export module napi_js:visit;
import :utility;
import :value;
import std;
import util;
import v8;

namespace js::napi {

// Instantiated in the acceptor that corresponds to a napi visitor
struct reference_map_type {
		template <class Type>
		using type = napi::unordered_value_map<Type>;
};

// Visitor which may visit only property names. `symbol` should probably be rethought since they are
// not cloneable.
template <class Visit>
struct visit_property_name {
	public:
		explicit visit_property_name(Visit& visit) : visit_{visit} {}

		template <class Accept>
		auto operator()(napi_value subject, const Accept& accept) -> accept_target_t<Accept> {
			return visit_.get().lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				auto accept_as = [ & ]<class Tag>(Tag tag) -> auto {
					auto value = bound_value{napi_env{visit_.get()}, value_of<Tag>::from(subject)};
					return accept(tag, *this, value);
				};
				switch (napi::invoke(napi_typeof, napi_env{visit_.get()}, subject)) {
					case napi_number: return accept_as(number_tag_of<std::int32_t>{});
					case napi_string: return accept_as(string_tag_of<char16_t>{});
					case napi_symbol: return accept_as(symbol_tag{});
					default: std::unreachable();
				}
			});
		}

		[[nodiscard]] auto environment() const -> auto& { return visit_.get().environment(); }

	private:
		std::reference_wrapper<Visit> visit_;
};

// `visit_key_literal` instantiation which creates property keys
template <auto Key>
struct visit_napi_key_literal {
	public:
		explicit constexpr visit_napi_key_literal(auto* /*transfer*/) {}

		auto operator()(const auto& /*could_be_literally_anything*/, const auto& accept_or_visit) -> napi_value {
			const auto make = util::overloaded{
				[](auto& env, std::string_view subject) -> napi::value_of<string_tag_of<char>> {
					auto* value = napi::invoke(node_api_create_property_key_latin1, napi_env{env}, subject.data(), subject.length());
					return napi::value_of<string_tag_of<char>>::from(value);
				},
				[](auto& env, std::u16string_view subject) -> napi::value_of<string_tag_of<char16_t>> {
					auto* value = napi::invoke(node_api_create_property_key_utf16, napi_env{env}, subject.data(), subject.length());
					return napi::value_of<string_tag_of<char16_t>>::from(value);
				},
				[](auto& env, std::u8string_view subject) -> napi::value_of<string_tag_of<char8_t>> {
					auto* value = napi::invoke(node_api_create_property_key_utf8, napi_env{env}, reinterpret_cast<const char*>(subject.data()), subject.length());
					return napi::value_of<string_tag_of<char8_t>>::from(value);
				},
			};
			if (local_key_ == napi_value{}) {
				constexpr auto key = util::make_consteval_string_view(Key);
				auto& environment = accept_or_visit.environment();
				auto storage = environment.string_table_storage(Key);
				if (storage) {
					auto& reference = *storage;
					if (reference) {
						local_key_ = reference.get(environment);
					} else {
						auto value = make(environment, key);
						reference.reset(environment, value);
						local_key_ = napi_value{value};
					}
				} else {
					return make(environment, key);
				}
			}
			return local_key_;
		}

	private:
		napi_value local_key_{};
};

// Base napi visitor implementing all functionality. Napi doesn't give us granular information like
// "is this a latin1 string" and all checks must be made at once. So it's structured it great deal
// differently than the v8 visitor.
template <auto_environment Environment, class Ref>
struct visit_value;

template <class Meta>
using visit_value_with = visit_value<
	typename Meta::visit_context_type,
	typename Meta::accept_reference_type>;

template <auto_environment Environment, class Reference>
struct visit_value : reference_map_t<Reference, reference_map_type> {
	public:
		using reference_map_t<Reference, reference_map_type>::lookup_or_visit;

		visit_value(auto* /*transfer*/, Environment& env) :
				env_{env} {}

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
			return immediate(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<undefined_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return immediate(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<boolean_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return immediate(subject, accept);
		}

		template <class Accept>
		auto operator()(value_of<number_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (maybe_is_number_int32(subject).value_or(false)) {
				return immediate(value_of<number_tag_of<std::int32_t>>::from(subject), accept);
			} else {
				return immediate(value_of<number_tag_of<double>>::from(subject), accept);
			}
		}

		template <class Accept, class Type>
		auto operator()(value_of<number_tag_of<Type>> subject, const Accept& accept) -> accept_target_t<Accept> {
			return immediate(subject, accept);
		}

		// General purpose visit operation which actually performs the type check
		template <class Accept>
		auto operator()(napi_value subject, const Accept& accept) -> accept_target_t<Accept> {

			// Check the reference map, and lookup type via napi
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				// This is the pure-napi implementation
				auto slow_path = util::overloaded{
					[ & ]() -> accept_target_t<Accept> {
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
								return immediate(value_of<symbol_tag>::from(subject), accept);
							case napi_object:
								return immediate(value_of<object_tag>::from(subject), accept);
							case napi_function:
								return immediate(value_of<function_tag>::from(subject), accept);
							case napi_external:
								return immediate(value_of<external_tag>::from(subject), accept);
							case napi_bigint:
								return immediate(value_of<bigint_tag>::from(subject), accept);
						}
						std::unreachable();
					},

					[](auto /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },
				};

				// Universal fast checks
				auto fast_path = util::overloaded{
					[ & ](undefined_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_undefined(napi_env{*this}, subject) ? (*this)(value_of<undefined_tag>::from(subject), accept) : next();
					},
					[ & ](null_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_null(napi_env{*this}, subject) ? (*this)(value_of<null_tag>::from(subject), accept) : next();
					},
					[ & ](boolean_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						if (fast_is_false(napi_env{*this}, subject)) {
							return (*this)(value_of<false_tag>::from(subject), accept);
						} else if (fast_is_true(napi_env{*this}, subject)) {
							return (*this)(value_of<true_tag>::from(subject), accept);
						} else {
							return next();
						}
					},
				};

				// Extended fast checks
				auto extended_fast_path = util::overloaded{
					[ & ](number_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_number(subject) ? (*this)(value_of<number_tag>::from(subject), accept) : next();
					},
					[ & ](string_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_string(subject) ? immediate(value_of<string_tag>::from(subject), accept) : next();
					},
					[ & ](bigint_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_bigint(subject) ? immediate(value_of<bigint_tag>::from(subject), accept) : next();
					},
					[ & ](date_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return napi::invoke(napi_is_date, napi_env{*this}, subject) ? immediate(value_of<date_tag>::from(subject), accept) : next();
					},
					[ & ](list_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_array(subject) ? immediate(value_of<list_tag>::from(subject), accept) : next();
					},
					[ & ](object_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						// nb: You can't really skip the subsequent is promise, is date, is arraybuffer, etc
						// checks. We don't really want to accept those types here, I don't think..
						return fast_is_object(subject) ? immediate(value_of<object_tag>::from(subject), accept) : next();
					},
					[ & ](function_tag /*tag*/, auto next) -> accept_target_t<Accept> {
						return fast_is_function(subject) ? immediate(value_of<function_tag>::from(subject), accept) : next();
					},

					// Skip these, otherwise `number_tag` and `string_tag` are invoked twice, which we don't
					// differentiate between.
					[]<class Type>(number_tag_of<Type> /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },
					[]<class Type>(string_tag_of<Type> /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },
				};

				if (has_extended_fast_is_functions) {
					auto traverse = util::overloaded{fast_path, extended_fast_path, slow_path};
					return util::template_traverse(accept_tags_of_v<Accept>, traverse);
				} else {
					auto traverse = util::overloaded{fast_path, slow_path};
					return util::template_traverse(accept_tags_of_v<Accept>, traverse);
				}
			});
		}

		template <class Accept>
		auto operator()(value_of<data_block_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return lookup_or_visit(subject, [ & ]() -> accept_target_t<Accept> {
				return util::template_traverse(
					accept_tags_of_v<Accept>,
					util::overloaded{
						// Fast paths
						[ & ](data_block_tag /*tag*/, auto /*next*/) -> accept_target_t<Accept> { return accept_tagged(subject, accept); },
						[ & ](array_buffer_tag /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },
						[ & ](shared_array_buffer_tag /*tag*/, auto next) -> accept_target_t<Accept> { return next(); },

						// Slow path
						[ & ]() -> accept_target_t<Accept> {
							return immediate(subject, accept);
						}
					}
				);
			});
		}

		// extras
		[[nodiscard]] auto environment() const -> Environment& { return env_; }
		explicit operator napi_env() const { return napi_env{env_.get()}; }
		consteval static auto types(auto /*recursive*/) { return util::type_pack{}; }

	private:
		// I think this only applies to `symbol_tag`
		template <class Accept, class Tag>
			requires std::is_convertible_v<Tag, primitive_tag>
		auto immediate(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// strings
		template <class Accept>
		auto immediate(value_of<string_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (maybe_is_string_latin1(subject).value_or(false)) {
				return accept_tagged(value_of<string_tag_of<char>>::from(subject), accept);
			} else {
				return accept_tagged(value_of<string_tag_of<char16_t>>::from(subject), accept);
			}
		}

		// bigint
		template <class Accept>
		auto immediate(value_of<bigint_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(value_of<bigint_tag_of<bigint>>::from(subject), accept);
		}

		// date
		template <class Accept>
		auto immediate(value_of<object_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (napi::invoke(napi_is_array, napi_env{*this}, subject)) {
				return immediate(value_of<list_tag>::from(subject), accept);
			} else if (napi::invoke(napi_is_date, napi_env{*this}, subject)) {
				return immediate(value_of<date_tag>::from(subject), accept);
			} else if (napi::invoke(napi_is_typedarray, napi_env{*this}, subject)) {
				return immediate(value_of<typed_array_tag>::from(subject), accept);
			} else if (napi::invoke(napi_is_dataview, napi_env{*this}, subject)) {
				return immediate(value_of<data_view_tag>::from(subject), accept);
			} else if (maybe_is_shared_array_buffer(env_.get(), subject).value_or(false)) {
				return immediate(value_of<shared_array_buffer_tag>::from(subject), accept);
			} else if (is_object_array_buffer(env_.get(), subject)) {
				return immediate(value_of<array_buffer_tag>::from(subject), accept);
			} else if (napi::invoke(napi_is_promise, napi_env{*this}, subject)) {
				return immediate(value_of<promise_tag>::from(subject), accept);
			} else {
				return immediate(value_of<dictionary_tag>::from(subject), accept);
			}
		}

		// date
		template <class Accept>
		auto immediate(value_of<date_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// data blocks
		template <class Accept>
		auto immediate(value_of<data_block_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			if (is_data_block_array_buffer(env_.get(), subject)) {
				return immediate(value_of<array_buffer_tag>::from(subject), accept);
			} else {
				return immediate(value_of<shared_array_buffer_tag>::from(subject), accept);
			}
		}

		template <class Accept, class Tag>
			requires std::is_convertible_v<Tag, data_block_tag>
		auto immediate(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// array buffer views
		template <class Accept>
		auto immediate(value_of<typed_array_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto bound_subject_variant = bound_value_for_typed_array::make_bound(environment(), subject);
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

		// externals
		template <class Accept>
		auto immediate(value_of<external_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// promise
		template <class Accept>
		auto immediate(value_of<promise_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// function
		template <class Accept>
		auto immediate(value_of<function_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept_tagged(subject, accept);
		}

		// array
		template <class Accept>
		auto immediate(value_of<list_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto target = napi::bound_value{napi_env{*this}, value_of<list_tag>::from(subject)};
			auto visit_entry = visit_entry_pair<visit_property_name<visit_value>, visit_value&>{*this};
			return accept(list_tag{}, visit_entry, target);
		}

		// object / record
		template <class Accept>
		auto immediate(value_of<dictionary_tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			auto target = napi::bound_value{napi_env{*this}, value_of<dictionary_tag>::from(subject)};
			auto visit_entry = visit_entry_pair<visit_property_name<visit_value>, visit_value&>{*this};
			return accept(dictionary_tag{}, visit_entry, target);
		}

		// Convenience function which wraps in `napi::bound_value` and invokes `accept`.
		template <class Tag, class Accept>
		[[nodiscard]] auto accept_tagged(value_of<Tag> subject, const Accept& accept) -> accept_target_t<Accept> {
			return accept(Tag{}, *this, napi::bound_value{environment(), subject});
		}

		std::reference_wrapper<Environment> env_;
};

} // namespace js::napi

namespace js {

// Napi value visitor entry
template <class Meta>
struct visit<Meta, napi_value> : napi::visit_value_with<Meta> {
		using napi::visit_value_with<Meta>::visit_value_with;
};

template <class Meta, class Tag>
struct visit<Meta, napi::value_of<Tag>> : napi::visit_value_with<Meta> {
		using napi::visit_value_with<Meta>::visit_value_with;
};

// Object key maker via napi
template <auto Key>
struct visit_key_literal<Key, napi_value> : napi::visit_napi_key_literal<Key> {
		using napi::visit_napi_key_literal<Key>::visit_napi_key_literal;
};

} // namespace js

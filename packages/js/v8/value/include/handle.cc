module;
#include <concepts>
#include <tuple>
#include <type_traits>
export module ivm.iv8:handle;
import v8;

export namespace ivm::js::iv8 {

struct handle_env {
		handle_env() = default;
		handle_env(v8::Isolate* isolate, v8::Local<v8::Context> context) :
				isolate{isolate},
				context{context} {}

		v8::Isolate* isolate{};
		v8::Local<v8::Context> context;
};

// Wrapper on top of `v8::Local` which carries around arbitrary data and passes it along to methods
// and casting operations on the underlying pointer.
template <class Type, class... Extra>
class handle : public v8::Local<Type> {
	public:
		using v8::Local<Type>::Local;

		handle(const v8::Local<Type>& local, handle_env env, auto&&... extra) :
				v8::Local<Type>{local},
				env_{env},
				extra_{std::forward<decltype(extra)>(extra)...} {}

		explicit handle(const handle<Type>& local, auto&&... extra) :
				v8::Local<Type>{local},
				env_{local.env()},
				extra_{std::forward<decltype(extra)>(extra)...} {}

		template <class To>
		auto to() {
			return handle<To>{v8::Local<Type>::template As<To>(), env_};
		}

		[[nodiscard]] auto env() const { return env_; }

		// Forward cast operators to the underlying method `materialize(std::type_identity<To>, ...)`
		template <class To>
		operator To() const // NOLINT(google-explicit-constructor)
			requires requires(Type that, handle_env env, Extra&... extra) {
				{ that.materialize(std::type_identity<To>{}, env, extra...) } -> std::same_as<To>;
			} {
			// nb: Inner lambda disambiguates overloads
			return apply_handle([](auto* that, auto&&... args) {
				return that->materialize(std::type_identity<To>{}, std::forward<decltype(args)>(args)...);
			});
		}

		// Forward iteration to underlying value
		[[nodiscard]] auto begin() const -> decltype(auto)
			requires std::invocable<decltype(&Type::begin), const Type&, handle_env, const Extra&...> {
			return apply_handle(&Type::begin);
		}

		[[nodiscard]] auto end() const -> decltype(auto)
			requires std::invocable<decltype(&Type::begin), const Type&, handle_env, const Extra&...> {
			return apply_handle(&Type::end);
		}

		[[nodiscard]] auto into_range() const -> decltype(auto)
			requires std::invocable<decltype(&Type::into_range), Type&, handle_env, Extra&...> {
			auto* non_const = const_cast<handle*>(this); // NOLINT(cppcoreguidelines-pro-type-const-cast)
			return non_const->apply_handle(&Type::into_range);
		}

		auto get(auto&&... args) -> decltype(auto)
			requires std::invocable<decltype(&Type::get), Type&, handle_env, Extra&..., decltype(args)...> {
			return apply_handle(&Type::get, std::forward<decltype(args)>(args)...);
		}

		auto has(auto&&... args) -> decltype(auto)
			requires std::invocable<decltype(&Type::has), Type&, handle_env, Extra&..., decltype(args)...> {
			return apply_handle(&Type::has, std::forward<decltype(args)>(args)...);
		}

	private:
		auto apply_handle(auto fn, auto&&... args) -> decltype(auto) {
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(**this, env_),
					std::tuple<Extra&...>(extra_),
					std::forward_as_tuple(args...)
				)
			);
		}

		auto apply_handle(auto fn, auto&&... args) const -> decltype(auto) {
			return std::apply(
				fn,
				std::tuple_cat(
					std::forward_as_tuple(**this, env_),
					std::tuple<const Extra&...>(extra_),
					std::forward_as_tuple(args...)
				)
			);
		}

		handle_env env_;
		std::tuple<Extra...> extra_;
};

// `Local` wrapper for primitive types which don't need extra pointers to materialize values.
// Basically only integral values and externals.
template <class Type>
class handle_cast : public v8::Local<Type> {
	public:
		explicit handle_cast(const v8::Local<Type>& local) :
				v8::Local<Type>{local} {}

		template <class To>
		operator To() const // NOLINT(google-explicit-constructor)
			requires requires(Type that) {
				{ that.materialize(std::type_identity<To>{}) } -> std::same_as<To>;
			} {
			return (*this)->materialize(std::type_identity<To>{});
		}
};

} // namespace ivm::js::iv8

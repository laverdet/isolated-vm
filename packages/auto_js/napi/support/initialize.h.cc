export module napi_js:initialize;
import :environment;
import :value;
import nodejs;
import std;
import util;

namespace js::napi {

namespace detail {

export class initialize_require : util::non_copyable {
	protected:
		~initialize_require();

	public:
		explicit initialize_require(int /*nothing*/, initialize_require& instance);
		virtual auto operator()(napi_env env, napi_value exports) -> void = 0;
		static auto initialize(napi_env env, napi_value exports) -> void;
};

} // namespace detail

export template <class Environment, class Make, class... Args>
class napi_js_module final : private detail::initialize_require {
	public:
		explicit napi_js_module(std::type_identity<Environment> /*env*/, Make make_exports, Args... args) :
				detail::initialize_require{0, *this},
				make_exports_{std::move(make_exports)},
				args_{std::move(args)...} {}

	private:
		auto operator()(napi_env env, napi_value exports) -> void override {
			// construct environment, set napi instance data w/ finalizer
			const auto [... indices ] = util::sequence<sizeof...(Args)>;
			auto& client_environment = environment::make_and_set_environment<Environment>(env, std::get<indices>(std::move(args_))...);
			// assign export descriptor to napi-constructor namespace object
			auto exports_local = value_of<dictionary_tag>::from(exports);
			exports_local.assign(client_environment, std::move(make_exports_)(client_environment));
		}

		Make make_exports_;
		std::tuple<Args...> args_;
};

} // namespace js::napi

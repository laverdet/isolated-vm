module;
#include <string>
#include <type_traits>
export module backend_napi_v8.environment;
import isolated_v8;
import isolated_js;
import napi_js;
import ivm.utility;
import nodejs;
using namespace js;
using namespace std::string_literals;

namespace backend_napi_v8 {

// Storage for string literals used in this module
constexpr auto string_literals = util::sealed_map{
	std::type_identity<napi::reference<string_tag>>{},
	"clock"s,
	"column"s,
	"epoch"s,
	"interval"s,
	"line"s,
	"location"s,
	"name"s,
	"origin"s,
	"randomSeed"s,
	"specifier"s,
	"type"s,
};

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment
		: util::non_moveable,
			public napi::environment_of<environment>,
			public napi::uv_schedulable {
	public:
		explicit environment(napi_env env);
		~environment();

		auto cluster() -> isolated_v8::cluster& { return cluster_; }

		// Lookup `reference<T>` for the given literal
		template <util::string_literal Value>
		auto global_storage(util::value_constant<Value> /*value*/) -> auto& {
			constexpr auto index = string_literals.lookup(std::string_view{Value});
			static_assert(index, "String literal is missing in storage");
			// static_assert(index, std::format("String literal '{}' is missing in storage", Value.data()));
			return string_literal_storage_.at(index).second;
		}

		static auto get(napi_env env) -> environment&;

	private:
		isolated_v8::cluster cluster_;
		util::copy_of<&string_literals> string_literal_storage_;
};

} // namespace backend_napi_v8

module;
#include <string>
#include <type_traits>
export module backend_napi_v8:environment;
import isolated_js;
import isolated_v8;
import ivm.utility;
import napi_js;
import nodejs;
using namespace js;
using namespace std::string_literals;

namespace backend_napi_v8 {

// Storage for string literals used in this module
constexpr auto string_literals = util::sealed_map{
	std::type_identity<napi::reference<string_tag>>{},
	"clock"s,
	"column"s,
	"complete"s,
	"epoch"s,
	"error"s,
	"interval"s,
	"line"s,
	"location"s,
	"name"s,
	"origin"s,
	"randomSeed"s,
	"result"s,
	"specifier"s,
	"timeout"s,
	"type"s,
};

// Instance of the `isolated-vm` module, once per nodejs environment.
export class environment : public napi::environment_of<environment> {
	public:
		explicit environment(napi_env env) : environment_of{env} {}

		auto cluster() -> isolated_v8::cluster& { return cluster_; }

		// Lookup `reference<T>` for the given literal
		template <auto Value>
		auto global_storage(util::constant_wrapper<Value> value) -> auto& {
			constexpr auto index = string_literals.lookup(std::string_view{value});
			static_assert(index, "String literal is missing in storage");
			// static_assert(index, std::format("String literal '{}' is missing in storage", Value.data()));
			return string_literal_storage_.at(index).second;
		}

	private:
		isolated_v8::cluster cluster_;
		util::copy_of<string_literals> string_literal_storage_;
};

} // namespace backend_napi_v8

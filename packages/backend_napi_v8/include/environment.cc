module;
#include <string>
#include <type_traits>
export module backend_napi_v8.environment;
import isolated_v8;
import isolated_js;
import napi_js;
import ivm.utility;
import nodejs;
import v8;
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

		auto make_global_literal(auto literal) {
			auto& storage = literal_storage(literal);
			if (storage) {
				return storage.get(*this);
			} else {
				using value_type = std::decay_t<decltype(storage)>::value_type;
				// `napi_value` acceptor is used since the `value<T>` actually only accepts already created
				// values. These should be unified!
				value_type value = value_type::from(transfer_in_strict<napi_value>(*literal, *this));
				storage.reset(*this, value);
				return value;
			}
		}

		static auto get(napi_env env) -> environment&;

	private:
		// Lookup `reference<T>` for the given literal
		template <util::string_literal Value>
		auto literal_storage(value_literal<Value> /*value*/) -> auto& {
			constexpr auto index = string_literals.lookup(std::string_view{Value});
			static_assert(index, "String literal is missing in storage");
			// static_assert(index, std::format("String literal '{}' is missing in storage", Value.data()));
			return string_literal_storage_.at(index).second;
		}

		isolated_v8::cluster cluster_;
		v8::Isolate* isolate_;
		std::decay_t<decltype(string_literals)> string_literal_storage_{string_literals};
};

} // namespace backend_napi_v8

module;
#include <functional>
#include <tuple>
#include <utility>
export module ivm.isolated_v8:script;
import :agent;
import :realm;
import ivm.iv8;
import ivm.utility;
import ivm.value;
import v8;

namespace ivm {

export class source_location {
	public:
		source_location() = default;
		source_location(int line, int column) : // NOLINT(bugprone-easily-swappable-parameters)
				line_{line},
				column_{column} {
		}

		[[nodiscard]] auto line() const -> int;
		[[nodiscard]] auto column() const -> int;

	private:
		int line_{};
		int column_{};
};

export class source_origin : util::non_moveable {
	public:
		source_origin() = default;
		source_origin(v8::Local<v8::String> resource_name, source_location location);
		source_origin(agent::lock& agent, auto&& resource_name, source_location location);

		[[nodiscard]] auto location() const -> source_location;
		[[nodiscard]] auto resource_name() const -> v8::Local<v8::String>;

	private:
		v8::Local<v8::String> resource_name_;
		source_location location_;
};

export class script : util::non_copyable {
	public:
		script() = delete;
		script(agent::lock& agent, v8::Local<v8::UnboundScript> script);

		auto run(realm::scope& realm_scope) -> value::value_t;
		static auto compile(agent::lock& agent, auto&& code_string, const source_origin& source_origin) -> script;

	private:
		static auto compile(agent::lock& agent, v8::Local<v8::String> code_string, const source_origin& source_origin) -> script;

		v8::Global<v8::UnboundScript> unbound_script_;
};

source_origin::source_origin(agent::lock& agent, auto&& resource_name, source_location location) :
		source_origin{
			std::invoke([ & ]() {
				auto maybe_local = value::transfer_strict<v8::MaybeLocal<v8::String>>(
					std::forward<decltype(resource_name)>(resource_name),
					std::tuple{},
					std::tuple{agent->isolate()}
				);
				v8::Local<v8::String> local{};
				std::ignore = maybe_local.ToLocal(&local);
				return local;
			}),
			location
		} {
}

auto script::compile(agent::lock& agent, auto&& code_string, const source_origin& source_origin) -> script {
	auto local_code_string = value::transfer_strict<v8::Local<v8::String>>(
		std::forward<decltype(code_string)>(code_string),
		std::tuple{},
		std::tuple{agent->isolate()}
	);
	return script::compile(agent, local_code_string, source_origin);
}

} // namespace ivm

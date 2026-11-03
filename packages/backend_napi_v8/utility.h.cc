export module backend_napi_v8:utility;
import :environment;
import auto_js;
import napi_js;
import std;
import util;
import v8_js;

namespace backend_napi_v8 {
using namespace js;

// `structuredClone`-style transfer options
struct transfer_options : js::optional_constructible {
		using js::optional_constructible::optional_constructible;
		std::optional<js::napi::value_of<js::list_tag>> transfer;

		constexpr static auto struct_template = js::struct_template{
			js::struct_member{util::cw<"transfer">, &transfer_options::transfer},
		};
};

// Wrapper for results from a function which could throw on user-supplied conditions
template <class Expected>
struct normal_completion_record {
	public:
		explicit normal_completion_record(Expected result) : result_{std::move(result)} {}

		[[nodiscard]] auto value() && -> Expected { return std::move(result_); }

		constexpr static auto variant = js::discriminated_alternative{util::cw<"complete">, util::cw<true>};
		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"result">, util::fn<&normal_completion_record::value>},
		};

	private:
		Expected result_;
};

struct throw_completion_record {
	public:
		explicit throw_completion_record(js::error_value error) : error_{std::move(error)} {}
		[[nodiscard]] auto error() && -> js::error_value { return std::move(error_); }

		constexpr static auto variant = js::discriminated_alternative{util::cw<"complete">, util::cw<false>};
		constexpr static auto struct_template = js::struct_template{
			js::struct_accessor{util::cw<"error">, util::fn<&throw_completion_record::error>},
		};

	private:
		js::error_value error_;
};

template <class Expected>
using completion_record = std::expected<normal_completion_record<Expected>, throw_completion_record>;

// Wraps a `std::expected` operation result into a `completion_record`
template <class Expected>
auto make_completion_record(std::expected<Expected, js::error_value> result) -> completion_record<Expected> {
	return completion_record<Expected>{std::move(result)};
}

// Requests termination after the given timeout duration. The timer is registered as the current
// execution timeout so that `execution_timeout_suspension` can pause it.
class execution_timeout : util::non_moveable {
	public:
		execution_timeout(js::iv8::isolated::executor& executor, js::js_clock::duration duration);
		~execution_timeout();

	private:
		class terminate {
			public:
				explicit terminate(v8::Isolate* isolate) : isolate_{isolate} {}
				auto operator()() const noexcept -> void;

			private:
				v8::Isolate* isolate_;
		};

		std::reference_wrapper<js::iv8::isolated::executor> executor_;
		util::timer_stop_token timer_;
		util::scope_stack<util::timer_stop_token*> scope_;
		std::optional<util::inplace_stop_callback<terminate>> callback_;
};

// Pauses the current execution timeout, if any, for the duration of the scope. Used while a script
// is blocked waiting on the host.
class execution_timeout_suspension : util::non_moveable {
	public:
		execution_timeout_suspension();
		~execution_timeout_suspension();

	private:
		util::timer_stop_token* timer_;
};

} // namespace backend_napi_v8

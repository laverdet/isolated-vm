module;
#include "auto_js/export_tag.h"
export module auto_js:intrinsics.error;
import :enum_.types;
import std;
import util;

namespace js {

// Specified JavaScript error names
export enum class error_name : std::uint8_t {
	error,
	eval_error,
	range_error,
	reference_error,
	syntax_error,
	type_error,
	uri_error
};

// Base transferred JavaScript -> C++ error type. This can be caught.
export class EXPORT error : public std::exception {
	public:
		explicit error(error_name name) : name_{name} {}

		[[nodiscard]] auto name() const -> error_name { return name_; }
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[JavaScript Error]"; };
		[[nodiscard]] virtual auto message() const -> std::u16string = 0;
		[[nodiscard]] virtual auto stack() const -> std::u16string { return {}; }

	private:
		error_name name_;
};

// JavaScript Error value. Note that `cause` is not transferred, which would make this structure
// circular.
export class error_value {
	public:
		explicit error_value(const error& error) :
				error_value{error.name(), error.message(), error.stack()} {}

		error_value(error_name name, std::u16string message, std::u16string stack = {}) :
				name_{name},
				message_{std::move(message)},
				stack_{std::move(stack)} {}

		[[nodiscard]] auto name() const -> error_name { return name_; }
		[[nodiscard]] auto message() const -> std::u16string { return message_; }
		[[nodiscard]] auto stack() const -> std::u16string { return stack_; }

	private:
		error_name name_;
		std::u16string message_;
		std::u16string stack_;
};

// Error with a given constructor bound
template <error_name Type>
class specific_error_value : public error {
	public:
		explicit specific_error_value(std::u16string message) :
				error{Type},
				message_{std::move(message)} {}

		[[nodiscard]] auto message() const -> std::u16string final { return message_; }

	private:
		std::u16string message_;
};

// Error which can be thrown from C++
export using runtime_error = specific_error_value<error_name::error>;
export using range_error = specific_error_value<error_name::range_error>;
export using type_error = specific_error_value<error_name::type_error>;

} // namespace js

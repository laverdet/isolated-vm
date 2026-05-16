module;
#include "auto_js/export_tag.h"
export module auto_js:intrinsics.error;
import :enum_.types;
import std;

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
		explicit error(error_name name);

		[[nodiscard]] auto name() const -> error_name;
		[[nodiscard]] auto what() const noexcept -> const char* final;
		[[nodiscard]] virtual auto message() const -> std::u16string = 0;
		[[nodiscard]] virtual auto stack() const -> std::u16string;

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

// ---

// Fix for the Windows issue noted below. There's not a lot of results for this exact error message
// and many are very old. It is probably another modules problem. Observed in clang v21.1.6 &
// v22.1.3. It only occurs when linking against isolated-vm from Windows, a case not covered by
// CI/CD.

// [...]/intrinsics/error.cc:23:12: error: dllimport cannot be applied to non-inline function definition
//  23 |  explicit error(error_name name) : name_{name} {}
//     |           ^

EXPORT inline error::error(error_name name) : name_{name} {}

EXPORT inline auto error::name() const -> error_name {
	return name_;
}

EXPORT inline auto error::what() const noexcept -> const char* {
	return "[JavaScript Error]";
};

EXPORT inline auto error::stack() const -> std::u16string {
	return {};
}

} // namespace js

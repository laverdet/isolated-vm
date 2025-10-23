module;
#include <cstdint>
#include <exception>
#include <string>
export module isolated_js:error;

namespace js {

// Base transferred JavaScript -> C++ error type. This can be caught.
export class error : public std::exception {
	public:
		enum class name_type : uint8_t {
			error,
			eval_error,
			range_error,
			reference_error,
			syntax_error,
			type_error,
			uri_error
		};

		explicit error(name_type name) : name_{name} {}

		[[nodiscard]] auto name() const -> name_type { return name_; }
		[[nodiscard]] auto what() const noexcept -> const char* final { return "[JavaScript Error]"; };
		[[nodiscard]] virtual auto message() const -> std::u16string = 0;
		[[nodiscard]] virtual auto stack() const -> std::u16string { return {}; }

	private:
		name_type name_;
};

// JavaScript Error value. Note that `cause` is not transferred, which would make this structure
// circular.
export class error_value : public error {
	public:
		explicit error_value(const error& error) :
				error_value{error.name(), error.message(), error.stack()} {}

		error_value(name_type name, std::u16string message, std::u16string stack = {}) :
				error{name},
				message_{std::move(message)},
				stack_{std::move(stack)} {}

		[[nodiscard]] auto message() const -> std::u16string final { return message_; }
		[[nodiscard]] auto stack() const -> std::u16string final { return stack_; }

	private:
		std::u16string message_;
		std::u16string stack_;
};

// Error with a given constructor bound
template <error::name_type Type>
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
export using range_error = specific_error_value<error_value::name_type::range_error>;
export using type_error = specific_error_value<error_value::name_type::type_error>;

} // namespace js

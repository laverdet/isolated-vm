module;
#include <concepts>
#include <tuple>
#include <type_traits>
export module v8_js.handle;
import v8_js.lock;
import v8;

namespace js::iv8 {

// Handle which requires the isolate
export class handle_with_isolate {
	public:
		handle_with_isolate() = default;
		explicit handle_with_isolate(const isolate_lock& lock) :
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }
		[[nodiscard]] auto witness() const -> isolate_implicit_witness_lock {
			return isolate_implicit_witness_lock{isolate_};
		}

	private:
		v8::Isolate* isolate_{};
};

// Handle which requires a context (and isolate)
export class handle_with_context : public handle_with_isolate {
	public:
		handle_with_context() = default;
		explicit handle_with_context(const context_lock& lock) :
				handle_with_isolate{lock},
				context_{lock.context()} {}

		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }
		[[nodiscard]] auto witness() const -> context_implicit_witness_lock {
			return context_implicit_witness_lock{handle_with_isolate::witness(), context_};
		}

	private:
		v8::Local<v8::Context> context_;
};

// Forward cast operators to the underlying method `materialize(std::type_identity<To>, ...)`
export template <class Type>
class handle_materializable {
	protected:
		friend Type;
		handle_materializable() = default;

	public:
		template <class To>
		// NOLINTNEXTLINE(google-explicit-constructor)
		[[nodiscard]] operator To(this auto&& self)
			requires requires {
				{ self.materialize(std::type_identity<To>{}) } -> std::same_as<To>;
			} {
			return self.materialize(std::type_identity<To>{});
		}
};

} // namespace js::iv8

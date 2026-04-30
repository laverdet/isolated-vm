export module v8_js:handle;
import :lock;
import v8;

namespace js::iv8 {

// Handle with no isolate requirements
template <class Type>
class handle_without_lock : public v8::Local<Type> {
	public:
		handle_without_lock() = default;
		explicit handle_without_lock(null_lock_witness /*lock*/, v8::Local<Type> local) :
				v8::Local<Type>(local) {}
};

// Handle which requires the isolate
template <class Type>
class handle_with_isolate : public v8::Local<Type> {
	public:
		// nb: It is default-constructible. So instead of copying the lock witness (which is not
		// default-constructible), the isolate is stored.
		handle_with_isolate() = default;
		explicit handle_with_isolate(isolate_lock_witness lock, v8::Local<Type> local) :
				v8::Local<Type>(local),
				isolate_{lock.isolate()} {}

		[[nodiscard]] auto isolate() const -> v8::Isolate* { return isolate_; }
		[[nodiscard]] auto witness() const -> isolate_lock_witness {
			return isolate_lock_witness::make_witness(isolate_);
		}

	private:
		v8::Isolate* isolate_{};
};

// Handle which requires a context (and isolate)
template <class Type>
class handle_with_context : public handle_with_isolate<Type> {
	public:
		handle_with_context() = default;
		explicit handle_with_context(context_lock_witness lock, v8::Local<Type> local) :
				handle_with_isolate<Type>{util::slice(lock), local},
				context_{lock.context()} {}

		[[nodiscard]] auto context() const -> v8::Local<v8::Context> { return context_; }
		// NOLINTNEXTLINE(bugprone-derived-method-shadowing-base-method)
		[[nodiscard]] auto witness() const -> context_lock_witness {
			return context_lock_witness::make_witness(handle_with_isolate<Type>::witness(), context_);
		}

	private:
		v8::Local<v8::Context> context_;
};

} // namespace js::iv8

module;
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>
export module napi_js.uv_scheduler;
import ivm.utility;
import nodejs;

namespace js::napi {

// Simple `uv_handle` wrapper which isolates all the pointer casting that needs to be done w/ the C
// API.
template <class Handle, class Type>
	requires std::is_trivial_v<Type> && (sizeof(Type) == sizeof(Handle::data))
class uv_typed_handle {
	public:
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator Handle*() { return reinterpret_cast<Handle*>(&handle_); }
		// NOLINTNEXTLINE(google-explicit-constructor)
		operator uv_handle_t*() { return reinterpret_cast<uv_handle_t*>(&handle_); }
		// Returns data lvalue
		auto data() -> Type& {
			void* ptr = static_cast<void*>(&handle_.data);
			return *static_cast<Type*>(ptr);
		}
		// Returns data rvalue
		[[nodiscard]] auto data() const -> Type { return static_cast<Type>(handle_.data); }
		[[nodiscard]] auto loop() const -> uv_loop_t* { return handle_.loop; }
		[[nodiscard]] auto type() const -> uv_handle_type { return handle_.type; }

	private:
		Handle handle_;
};

// `uv_handle_t` subtype wrapper with uv-owned shared memory. An open handle must be closed before
// the handle is destroyed or it is UB.
template <class Handle, class Type>
class uv_handle_of : public util::pointer_facade<uv_handle_of<Handle, Type>> {
	private:
		struct private_constructor {
				explicit private_constructor() = default;
		};
		using shared_ptr_type = std::shared_ptr<uv_handle_of>;
		using weak_ptr_type = std::weak_ptr<uv_handle_of>;
		// An open handle's `data` pointer refers to a `shared_ptr` of itself. A closed handle refers to
		// a `weak_ptr`, which will be deleted in the destructor.
		union shared_or_weak_ptr {
				shared_ptr_type* shared;
				weak_ptr_type* weak;
		};

	public:
		explicit uv_handle_of(const private_constructor& /*private*/, auto&&... args) :
				value_{std::forward<decltype(args)>(args)...} {}
		~uv_handle_of();
		uv_handle_of() = delete;
		uv_handle_of(const uv_handle_of&) = delete;
		auto operator=(const uv_handle_of&) -> uv_handle_of& = delete;

		auto operator*(this auto& self) -> auto& { return self.value_; }
		auto close();
		auto handle(this auto& self) -> auto& { return self.handle_; }
		auto open(const auto& init, uv_loop_t* loop, auto&&... args);

		static auto make(auto&&... args) -> shared_ptr_type;
		static auto unwrap(const Handle* handle) -> const shared_ptr_type&;

	private:
		uv_typed_handle<Handle, shared_or_weak_ptr> handle_;
		Type value_;
};

// Shareable libuv scheduler
export class uv_scheduler {
	private:
		using task_type = util::maybe_move_only_function<void()>;

	public:
		auto close() -> void;
		auto decrement_ref() -> void;
		auto increment_ref() -> void;
		auto open(uv_loop_t* loop) -> void;
		auto operator()(task_type task) -> void;

	private:
		struct locked_storage {
				std::vector<task_type> tasks;
				bool is_open{};
		};
		struct storage {
				storage() = default;
				util::lockable<locked_storage> shared;
				std::thread::id thread_id{std::this_thread::get_id()};
				int refs{};
		};
		using handle_type = uv_handle_of<uv_async_t, storage>;
		std::shared_ptr<handle_type> async_{handle_type::make()};
};

// An environment with an uv loop and scheduler
export class uv_schedulable {
	public:
		auto scheduler(this auto& self) -> auto& { return self.uv_scheduler_; }

	private:
		uv_scheduler uv_scheduler_;
};

// ---

template <class Handle, class Type>
uv_handle_of<Handle, Type>::~uv_handle_of() {
	delete handle_.data().weak;
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::close() {
	uv_close(handle(), [](uv_handle_t* handle) {
		delete static_cast<shared_ptr_type*>(std::exchange(handle->data, nullptr));
	});
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::open(const auto& init, uv_loop_t* loop, auto&&... args) {
	auto weak_ptr_ptr = std::unique_ptr<weak_ptr_type>{std::exchange(handle_.data().weak, nullptr)};
	auto shared_ptr_ptr = std::make_unique<shared_ptr_type>(*weak_ptr_ptr);
	if (init(loop, handle(), std::forward<decltype(args)>(args)...) == 0) {
		handle_.data().shared = shared_ptr_ptr.release();
	} else {
		handle_.data().weak = weak_ptr_ptr.release();
		throw std::runtime_error{"Failed to open async handle"};
	}
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::make(auto&&... args) -> shared_ptr_type {
	auto shared_with_block = std::make_shared<uv_handle_of>(private_constructor{}, std::forward<decltype(args)>(args)...);
	auto weak_ptr_ptr = std::make_unique<weak_ptr_type>(shared_with_block);
	shared_with_block->handle_.data().weak = weak_ptr_ptr.release();
	return shared_with_block;
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::unwrap(const Handle* handle) -> const shared_ptr_type& {
	return *static_cast<shared_ptr_type*>(handle->data);
}

} // namespace js::napi

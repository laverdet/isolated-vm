module;
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>
export module napi_js.uv_scheduler;
import ivm.utility;
import nodejs;

namespace js::napi {

// `uv_handle_t` subtype wrapper with uv-owned shared memory. An open handle must be closed before
// the handle is destroyed or it is UB.
template <class Handle, class Type>
class uv_handle_of : public util::pointer_facade<uv_handle_of<Handle, Type>>, util::non_moveable {
	private:
		struct private_ctor {
				explicit private_ctor() = default;
		};
		using shared_ptr_type = std::shared_ptr<uv_handle_of>;
		using weak_ptr_type = std::weak_ptr<uv_handle_of>;

	public:
		explicit uv_handle_of(const private_ctor& /*private*/, auto&&... args) :
				value_{std::forward<decltype(args)>(args)...} {}
		~uv_handle_of();
		uv_handle_of() = delete;
		uv_handle_of(const uv_handle_of&) = delete;
		auto operator=(const uv_handle_of&) -> uv_handle_of& = delete;

		auto operator*(this auto& self) -> auto& { return self.value_; }
		auto close();
		auto handle(this auto& self) -> auto* { return &self.handle_; }
		auto handle_base(this auto& self) -> auto* { return reinterpret_cast<uv_handle_t*>(&self.handle_); }
		auto open(const auto& init, uv_loop_t* loop, auto&&... args);

		static auto make(auto&&... args) -> shared_ptr_type;
		static auto unwrap(const Handle* handle) -> const shared_ptr_type&;

	private:
		Handle handle_;
		Type value_;
};

// Shareable libuv scheduler
export class uv_scheduler {
	private:
		using task_type = std::move_only_function<void()>;

	public:
		auto close() -> void;
		auto decrement_ref() -> void;
		auto increment_ref() -> void;
		auto open(uv_loop_t* loop) -> void;
		auto schedule(task_type task) -> void;

	private:
		using storage_type = util::lockable<std::vector<task_type>>;
		using handle_type = uv_handle_of<uv_async_t, storage_type>;
		std::shared_ptr<handle_type> async_{handle_type::make()};
		int refs_ = 0;
};

template <class Handle, class Type>
uv_handle_of<Handle, Type>::~uv_handle_of() {
	delete reinterpret_cast<weak_ptr_type*>(handle_.data);
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::close() {
	uv_close(handle_base(), [](uv_handle_t* handle) {
		delete reinterpret_cast<shared_ptr_type*>(std::exchange(handle->data, nullptr));
	});
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::open(const auto& init, uv_loop_t* loop, auto&&... args) {
	auto weak_ptr_ptr = std::invoke([ & ]() {
		auto* raw_ptr = reinterpret_cast<weak_ptr_type*>(std::exchange(handle_.data, nullptr));
		return std::unique_ptr<weak_ptr_type>{raw_ptr};
	});
	auto shared_ptr_ptr = std::make_unique<shared_ptr_type>(*weak_ptr_ptr);
	handle_.data = shared_ptr_ptr.get();
	if (init(loop, handle(), std::forward<decltype(args)>(args)...) == 0) {
		shared_ptr_ptr.release();
	} else {
		handle_.data = weak_ptr_ptr.release();
		throw std::runtime_error{"Failed to open async handle"};
	}
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::make(auto&&... args) -> shared_ptr_type {
	auto shared_with_block = std::make_shared<uv_handle_of>(private_ctor{}, std::forward<decltype(args)>(args)...);
	auto weak_ptr_ptr = std::make_unique<weak_ptr_type>(shared_with_block);
	shared_with_block->handle_.data = weak_ptr_ptr.release();
	return shared_with_block;
}

template <class Handle, class Type>
auto uv_handle_of<Handle, Type>::unwrap(const Handle* handle) -> const shared_ptr_type& {
	return *reinterpret_cast<shared_ptr_type*>(handle->data);
}

} // namespace js::napi

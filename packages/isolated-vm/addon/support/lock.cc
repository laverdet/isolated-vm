export module isolated_vm:support.lock_fwd;

namespace isolated_vm {

export class basic_lock {
	private:
		friend class basic_lock_implementation;
		friend class runtime_lock;
		explicit basic_lock(void* data);

	public:
		basic_lock(const basic_lock&) = delete;
		~basic_lock() = default;
		auto operator=(const basic_lock&) -> basic_lock& = delete;
		[[nodiscard]] auto witness() const;

	private:
		void* basic_data_;
};

export class runtime_lock : public basic_lock {
	private:
		friend class runtime_lock_implementation;
		explicit runtime_lock(void* basic_data, void* runtime_data);

	public:
		[[nodiscard]] auto witness() const;

	private:
		void* runtime_data_;
};

} // namespace isolated_vm

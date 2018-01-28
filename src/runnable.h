#pragma once

namespace ivm {
	// std::function<> must be copyable, so we use this where possible
	class Runnable {
		public:
			Runnable() = default;
			Runnable(const Runnable&) = delete;
			Runnable& operator= (const Runnable&) = delete;
			virtual ~Runnable() = default;
			virtual void Run() = 0;
	};
} // namespace ivm

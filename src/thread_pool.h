#pragma once
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <list>
#include <vector>

// This file contains no v8 code and is therefore free from v8's naming conventions

class thread_pool_t {
	public:
		typedef void(entry_t)(bool, void*);
		class affinity_t {
			friend class thread_pool_t;
			std::list<size_t> ids;
		};

	private:
		size_t desired_size;
		std::mutex mutex;
		size_t rr;

		struct thread_data_t {
			std::unique_ptr<std::condition_variable> cv;
			bool should_exit;
			entry_t* entry;
			void* param;
			thread_data_t() :
				cv(std::make_unique<std::condition_variable>()),
				should_exit(false), entry(nullptr), param(nullptr)
				{
			}
		};

		std::vector<thread_data_t> thread_data;
		std::vector<std::thread> threads;

		size_t new_thread() {
			// no lock needed here since this is only called from `exec` which will have a lock
			thread_data.emplace_back();
			size_t ii = thread_data.size() - 1;
			threads.emplace_back(std::thread([ this, ii ]() {
				std::unique_lock<std::mutex> lock(mutex);
				while (!thread_data[ii].should_exit) {
					if (thread_data[ii].entry) {
						entry_t* entry = thread_data[ii].entry;
						void* param = thread_data[ii].param;
						lock.unlock();
						entry(true, param);
						lock.lock();
						thread_data[ii].entry = nullptr;
						thread_data[ii].param = nullptr;
					} else {
						thread_data[ii].cv->wait(lock);
					}
				}
			}));
			return ii;
		}

	public:
		thread_pool_t(size_t desired_size) : desired_size(desired_size), rr(0) {}

		~thread_pool_t() {
			resize(0);
		}

		void exec(affinity_t& affinity, entry_t* entry, void* param) {
			std::unique_lock<std::mutex> lock(this->mutex);

			// First try to use an old thread
			int thread = -1;
			for (auto ii = affinity.ids.begin(); ii != affinity.ids.end(); ) {
				if (*ii >= thread_data.size()) {
					ii = affinity.ids.erase(ii);
					continue;
				}
				if (thread_data[*ii].entry == nullptr) {
					thread = *ii;
					if (ii != affinity.ids.begin()) {
						// Move last used thread to front
						affinity.ids.erase(ii);
						affinity.ids.push_front(thread);
					}
					break;
				}
				++ii;
			}

			if (thread == -1) {
				if (desired_size > threads.size()) {
					// Thread pool hasn't yet reached `desired_size`, so we can make a new thread
					thread = new_thread();
					affinity.ids.push_front(thread);
				} else {
					// Now try to re-use a non-busy thread
					size_t offset = rr++;
					for (size_t ii = 0; ii < thread_data.size(); ++ii) {
						size_t jj = (rr + ii + offset) % thread_data.size();
						if (thread_data[jj].entry == nullptr) {
							thread = jj;
							affinity.ids.push_front(thread);
							break;
						}
					}

					if (thread == -1) {
						// All threads are busy and pool is full, just run this in a new thread
						std::thread tmp_thread(std::bind(entry, false, param));
						tmp_thread.detach();
						return;
					}
				}
			}

			thread_data[thread].entry = entry;
			thread_data[thread].param = param;
			thread_data[thread].cv->notify_one();
		}

		void resize(size_t size) {
			std::unique_lock<std::mutex> lock(this->mutex);
			desired_size = size;
			if (thread_data.size() > desired_size) {
				for (size_t ii = desired_size; ii < thread_data.size(); ++ii) {
					thread_data[ii].should_exit = true;
					thread_data[ii].cv->notify_one();
				}
				lock.unlock();
				for (size_t ii = desired_size; ii < thread_data.size(); ++ii) {
					threads[ii].join();
				}
				thread_data.resize(desired_size);
				threads.resize(desired_size);
			}
		}
};

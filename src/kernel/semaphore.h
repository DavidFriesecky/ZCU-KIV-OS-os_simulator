#pragma once

#include <mutex>
#include <condition_variable>

class Semaphore {
	public:
		std::mutex mutex;
		std::condition_variable cond;
		int value;
		int waiting;

		Semaphore(int value);
		void P();
		void V();
};
#pragma once

#include "semaphore.h"

Semaphore::Semaphore(int value) {
	this->value = value;
	this->waiting = 0;
}

void Semaphore::P() {
	std::unique_lock<std::mutex> lock(mutex);

	if (value <= 0) {
		waiting++;
		cond.wait(lock);
		return;
	}

	value--;
}

void Semaphore::V() {
	std::unique_lock<std::mutex> lock(mutex);

	if (waiting > 0) {
		waiting--;
		cond.notify_one();
		return;
	}

	value++;
}
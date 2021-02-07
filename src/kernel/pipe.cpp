#pragma once

#include "pipe.h"

Pipe::Pipe(int buffer_size) {
	this->buffer_size = buffer_size;

	this->producer = new Semaphore(buffer_size);
	this->consumer = new Semaphore(0);

	closed_input = false;
	closed_output = false;
}

uint64_t Pipe::Produce(char* buffer, uint64_t buffer_length) {
	uint64_t produced = 0;

	for (uint64_t i = 0; i < buffer_length; i++) {
		if (closed_input) {
			return produced;
		}

		producer->P();

		std::unique_lock<std::mutex> lock(mutex);
		buffer_pipe.push_back(buffer[i]);
		lock.unlock();

		consumer->V();
		produced++;
	}

	return produced;
}

uint64_t Pipe::Consume(char* buffer, uint64_t buffer_length) {
	uint64_t consumed = 0;

	for (uint64_t i = 0; i < buffer_length; i++) {
		if (closed_output && buffer_pipe.empty()) {
			return consumed;
		}

		consumer->P();

		if (closed_output && buffer_pipe.empty()) {
			return consumed;
		}

		std::unique_lock<std::mutex> lock(mutex);
		buffer[i] = buffer_pipe.front();
		buffer_pipe.pop_front();
		lock.unlock();

		producer->V();
		consumed++;
	}

	return consumed;
}

void Pipe::Close(Pipe_Type pipe_type) {
	switch (pipe_type) {
		case Pipe_Type::Read:
			closed_input = true;
			break;
		case Pipe_Type::Write:
			closed_output = true;
			consumer->cond.notify_one();
			break;
	}
}
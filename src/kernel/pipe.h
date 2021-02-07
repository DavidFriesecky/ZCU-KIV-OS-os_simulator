#pragma once

#include "semaphore.h"
#include <list>

enum class Pipe_Type {
	Read = 0,
	Write
};

class Pipe {
	public:
		Semaphore *producer;
		Semaphore *consumer;
		
		int buffer_size;
		std::list<char> buffer_pipe;
		std::mutex mutex;

		bool closed_input;
		bool closed_output;

		Pipe(int buffer_size);

		uint64_t Produce(char* buffer, uint64_t buffer_length);
		uint64_t Consume(char* buffer, uint64_t buffer_length);
		
		void Close(Pipe_Type pipe_type);
};
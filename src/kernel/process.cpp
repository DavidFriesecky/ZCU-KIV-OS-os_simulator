#include "process.h"

Process::Process(const char* process_name, kiv_os::THandle pid) {
	std::string str_process_name = std::string(process_name);

	this->process_name = process_name;

	this->pid = pid;
	this->process_tid = 0;
	this->state = State::Ready;

	this->working_dir = "";
	this->working_dir_sector = 0;

	this->handle_in = 0;
	this->handle_out = 0;
}

Process::Process(const char* process_name, kiv_os::THandle pid, std::string working_dir, uint16_t working_dir_sector, kiv_os::THandle handle_in, kiv_os::THandle handle_out) {
	std::string str_process_name = std::string(process_name);

	this->process_name = process_name;

	this->pid = pid;
	this->process_tid = 0;
	this->state = State::Ready;

	if (working_dir.size() > 0) {
		this->working_dir = working_dir;
		this->working_dir_sector = working_dir_sector;
	} else {
		this->working_dir = "";
		this->working_dir_sector = 0;
	}

	this->handle_in = handle_in;
	this->handle_out = handle_out;
}

uint64_t Process::Create_Thread(kiv_os::THandle tid, kiv_os::TThread_Proc entry_point, kiv_hal::TRegisters regs, const char* args) {
	std::unique_ptr<Thread> thread = std::make_unique<Thread>(tid, pid, entry_point, regs, args);
	thread->Start();
	uint64_t std_tid = thread->std_tid;

	state = State::Running;

	if (this->process_tid == 0) {
		process_tid = tid;
	}

	threads.insert({ tid, std::move(thread) });

	return std_tid;
}

void Process::Join_thread(kiv_os::THandle tid, uint16_t exit_code) {
	if (process_tid == tid) {
		state = State::Exited;
	}

	threads[tid]->Join(exit_code);
}
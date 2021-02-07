#include "thread.h"

Thread::Thread(kiv_os::THandle tid, kiv_os::THandle pid) {
	this->tid = tid;
	this->pid = pid;
	this->std_tid = 0;

	this->state = State::Running;
	
	this->entry_point = nullptr;
	
	kiv_hal::TRegisters regs;
	regs.rax.x = 0;
	this->regs = regs;

	this->exit_code = -1;
	this->waked_by_handler = 0;
}

Thread::Thread(kiv_os::THandle tid, kiv_os::THandle pid, kiv_os::TThread_Proc entry_point, kiv_hal::TRegisters regs, const char* args) {
	this->tid = tid;
	this->pid = pid;
	this->std_tid = 0;

	this->state = State::Ready;
	
	this->entry_point = entry_point;

	this->regs = regs;
	this->args = args;

	this->exit_code = -1;
	this->waked_by_handler = 0;
}

Thread::~Thread() {
	if (std_thread.joinable()) {
		std_thread.detach();
	}
}

void Thread::Start() {
	state = State::Running;
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(args.data());
	std_thread = std::thread(entry_point, regs);
	std_tid = Get_STD_TID(std_thread.get_id());
}

void Thread::Stop() {
	std::unique_lock<std::mutex> lock(mutex);

	while (waiting_handles.size() > 0) {
		if (state == State::Running) {
			state = State::Blocked;
			cond.wait(lock);
		}
	}
}

void Thread::Restart(kiv_os::THandle waiting_tid) {
	std::lock_guard<std::mutex> lock(mutex);

	if (waiting_handles[waiting_tid] >= 0) {
		waiting_handles.erase(waiting_tid);
		
		cond.notify_all();

		if (state == State::Blocked) {
			state = State::Running;
			waked_by_handler = waiting_tid;
		}
	}
}

void Thread::Join(uint16_t exit_code) {
	this->exit_code = exit_code;
	this->state = State::Exited;

	if (std_thread.joinable()) {
		std_thread.detach();
	}
}

uint64_t Thread::Get_STD_TID(std::thread::id std_tid) {
	return std::hash<std::thread::id>()(std_tid);
}
#pragma once

#include "thread.h"

#include <map>
#include <string>

class Process {
	public:
		std::string process_name;
		kiv_os::THandle pid;
		State state;

		kiv_os::THandle process_tid;

		std::map<kiv_os::THandle, std::unique_ptr<Thread>> threads;

		std::string working_dir;
		uint16_t working_dir_sector;
		kiv_os::THandle handle_in;
		kiv_os::THandle handle_out;
		
		Process(const char* process_name, kiv_os::THandle pid);
		Process(const char* process_name, kiv_os::THandle pid, std::string working_dir, uint16_t working_dir_sector, kiv_os::THandle handle_in, kiv_os::THandle handle_out);
		uint64_t Create_Thread(kiv_os::THandle tid, kiv_os::TThread_Proc entry_point, kiv_hal::TRegisters regs, const char* args);
		void Join_thread(kiv_os::THandle tid, uint16_t exit_code);
};
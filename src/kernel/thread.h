#pragma once

#include "../api/api.h"
#include "state.h"

#include <thread>
#include <condition_variable>
#include <vector>
#include <map>

class Thread {
	public:
		kiv_os::THandle tid;
		kiv_os::THandle pid;
		State state;

		uint64_t std_tid;
		std::thread std_thread;
		kiv_os::TThread_Proc entry_point;

		kiv_hal::TRegisters regs;
		std::string args;
		
		std::map<kiv_os::THandle, uint64_t> waiting_handles;
		std::map<kiv_os::THandle, kiv_os::THandle> sleeped_handles;
		std::map<kiv_os::NSignal_Id, kiv_os::TThread_Proc> terminate_handles;
		
		uint16_t exit_code;
		std::condition_variable cond;
		std::mutex mutex;
		kiv_os::THandle waked_by_handler;

		Thread(kiv_os::THandle tid, kiv_os::THandle pid);
		Thread(kiv_os::THandle tid, kiv_os::THandle pid, kiv_os::TThread_Proc entry_point, kiv_hal::TRegisters regs, const char* args);
		~Thread();

		void Start();
		void Stop();
		void Restart(kiv_os::THandle waiting_tid);
		void Join(uint16_t exit_code);

		static uint64_t Get_STD_TID(std::thread::id std_tid);
};
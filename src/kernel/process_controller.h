#pragma once

#include "../api/api.h"
#include "process.h"
#include "thread.h"
#include "handles.h"

#include <map>
#include <mutex>

extern HMODULE User_Programs;

#define MIN_PID 0
#define MIN_TID 1024

#define MAX_PID_COUNT 1024
#define MAX_TID_COUNT 1024

class Process_Controller {
	public:
		bool sig_shutdown = false;

		std::mutex process_table_mutex;
		std::mutex stdtid_table_mutex;

		std::mutex used_pid_mutex;
		std::mutex used_tid_mutex;

		kiv_os::THandle free_pid = MIN_PID;
		kiv_os::THandle free_tid = MIN_TID;

		bool used_pid[MAX_PID_COUNT] = { false };
		bool used_tid[MAX_TID_COUNT] = { false };

		// PID -> Process
		std::map<kiv_os::THandle, std::unique_ptr<Process>> process_table = {};
		// TID -> PID
		std::map<kiv_os::THandle, kiv_os::THandle> tid_to_pid = {};
		// std_TID -> TID
		std::map<uint64_t, kiv_os::THandle> stdtid_to_tid = {};

		Process_Controller();

		void Clone(kiv_hal::TRegisters& regs);
		void Create_Process(kiv_hal::TRegisters& regs);
		void Create_Thread(kiv_hal::TRegisters& regs);

		void Wait_For(kiv_hal::TRegisters& regs);
		void Read_Exit_Code(kiv_hal::TRegisters& regs);
		void Exit(kiv_hal::TRegisters& regs);
		void Shutdown(kiv_hal::TRegisters& regs);
		void Register_Signal_Handler(kiv_hal::TRegisters& regs);

		void Handle_Process(kiv_hal::TRegisters& regs);

		void Notify_All(kiv_os::THandle tid);
		void Notify(kiv_os::THandle sleeped_tid, kiv_os::THandle waiting_tid);

		kiv_os::THandle Get_Free_PID();
		void Set_Free_PID(kiv_os::THandle pid);
		kiv_os::THandle Get_Free_TID();
		void Set_Free_TID(kiv_os::THandle tid);

		uint64_t Get_STD_TID(kiv_os::THandle tid);

		kiv_os::THandle Get_PID();
		kiv_os::THandle Get_TID();

		kiv_os::THandle Exist_TID(kiv_os::THandle tid);
};
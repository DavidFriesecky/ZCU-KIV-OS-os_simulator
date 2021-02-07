#include "process_controller.h"

Process_Controller::Process_Controller() {
	// PID -> Process
	std::map<kiv_os::THandle, std::unique_ptr<Process>> process_table;
	// TID -> PID
	std::map<kiv_os::THandle, kiv_os::THandle> tid_to_pid;
	// std_TID -> TID
	std::map<uint64_t, kiv_os::THandle> stdtid_to_tid;
}

void Process_Controller::Clone(kiv_hal::TRegisters &regs) {
	kiv_os::NClone clone = static_cast<kiv_os::NClone>(regs.rcx.r);

	switch (clone) {
		case kiv_os::NClone::Create_Process:
			Create_Process(regs);
			break;
		case kiv_os::NClone::Create_Thread:
			Create_Thread(regs);
			break;
	}
	return;
}

void Process_Controller::Create_Process(kiv_hal::TRegisters &regs) {
	std::lock_guard<std::mutex> lock(process_table_mutex);

	if (sig_shutdown) {
		return;
	}

	kiv_os::THandle pid = Get_Free_PID();
	kiv_os::THandle tid = Get_Free_TID();

	char* process_name = reinterpret_cast<char*>(regs.rdx.r);
	char* process_args = reinterpret_cast<char*>(regs.rdi.r);

	kiv_hal::TRegisters process_regs;
	process_regs.rax.x = regs.rbx.e >> 16;
	process_regs.rbx.x = regs.rbx.e & 0x0000FFFF;
	process_regs.rdi.r = regs.rdi.r;

	std::unique_ptr<Process> process = std::make_unique<Process>(process_name, pid, process_table[Get_PID()]->working_dir, process_table[Get_PID()]->working_dir_sector, process_regs.rax.x, process_regs.rbx.x);

	kiv_os::TThread_Proc entry_point = (kiv_os::TThread_Proc) GetProcAddress(User_Programs, process_name);
	uint64_t stdtid = process->Create_Thread(tid, entry_point, process_regs, process_args);
	process->process_tid = tid;

	stdtid_to_tid.insert(std::pair<uint64_t, kiv_os::THandle>(stdtid, tid));
	tid_to_pid.insert(std::pair<kiv_os::THandle, kiv_os::THandle>(tid, pid));
	process_table.insert(std::pair<kiv_os::THandle, std::unique_ptr<Process>>(pid, std::move(process)));

	regs.rax.x = tid;
}

void Process_Controller::Create_Thread(kiv_hal::TRegisters &regs) {
	std::lock_guard<std::mutex> lock(process_table_mutex);

	if (sig_shutdown) {
		return;
	}
	
	char* thread_args = reinterpret_cast<char*>(regs.rdi.r);

	kiv_hal::TRegisters thread_regs;
	thread_regs.rdi.r = regs.rdi.r;

	if (tid_to_pid.find(Get_TID()) == tid_to_pid.end()) {
		return;
	}

	kiv_os::THandle pid = tid_to_pid[Get_TID()];
	kiv_os::THandle tid = Get_Free_TID();

	if (process_table.find(pid) == process_table.end()) {
		return;
	}

	thread_regs.rax.x = process_table[pid]->handle_in;
	thread_regs.rbx.x = process_table[pid]->handle_out;

	kiv_os::TThread_Proc entry_point = reinterpret_cast<kiv_os::TThread_Proc>(regs.rdx.r);
	uint64_t stdtid = process_table[pid]->Create_Thread(tid, entry_point, thread_regs, thread_args);

	stdtid_to_tid.insert(std::pair<uint64_t, kiv_os::THandle>(stdtid, tid));
	tid_to_pid.insert(std::pair<kiv_os::THandle, kiv_os::THandle>(tid, pid));

	regs.rax.x = tid;
}

void Process_Controller::Wait_For(kiv_hal::TRegisters& regs) {
	std::condition_variable cond;

	kiv_os::THandle* handles = reinterpret_cast<kiv_os::THandle*>(regs.rdx.r);
	uint64_t handles_count = static_cast<uint64_t>(regs.rcx.r);

	kiv_os::THandle tid = Get_TID();
	kiv_os::THandle pid = Get_PID();

	std::unique_lock<std::mutex> lock_mutex(process_table_mutex);

	for (uint64_t i = 0; i < handles_count; i++) {
		kiv_os::THandle waiting_tid = handles[i];
		kiv_os::THandle waiting_pid = tid_to_pid[handles[i]];

		if (process_table.find(waiting_pid) == process_table.end() || process_table[waiting_pid]->threads.find(waiting_tid) == process_table[waiting_pid]->threads.end()) {
			lock_mutex.unlock();
			return;
		}

		if (process_table[waiting_pid]->state == State::Exited || process_table[waiting_pid]->threads[waiting_tid]->state == State::Exited) {
			regs.rax.x = Exist_TID(waiting_tid);
			lock_mutex.unlock();
			return;
		}

		process_table[pid]->threads[tid]->waiting_handles.insert(std::pair<kiv_os::THandle, uint64_t>(waiting_tid, i));
		process_table[waiting_pid]->threads[waiting_tid]->sleeped_handles.insert(std::pair<kiv_os::THandle, kiv_os::THandle>(tid, tid));
	}

	lock_mutex.unlock();

	process_table[pid]->threads[tid]->Stop();
	kiv_os::THandle signalized_handle;

	std::lock_guard<std::mutex> lock_guard(process_table_mutex);

	if (process_table.find(pid) == process_table.end())	{
		return;
	}

	if (process_table[pid]->threads.find(tid) == process_table[pid]->threads.end())	{
		return;
	}

	signalized_handle = Exist_TID(process_table[pid]->threads[tid]->waked_by_handler);

	regs.rax.x = signalized_handle;
}

void Process_Controller::Read_Exit_Code(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock(process_table_mutex);

	if (process_table.size() == 0) {
		return;
	}

	kiv_os::THandle tid = static_cast<kiv_os::THandle>(regs.rdx.x);
	uint64_t stdtid = Get_STD_TID(tid);

	if (tid_to_pid.find(tid) == tid_to_pid.end()) {
		return;
	}

	kiv_os::THandle pid = tid_to_pid[tid];
	uint16_t exit_code = 0;

	if (process_table.find(pid) == process_table.end()) {
		return;
	}

	if (process_table[pid]->threads.find(tid) == process_table[pid]->threads.end()) {
		return;
	}

	exit_code = process_table[pid]->threads[tid]->exit_code;

	if (process_table[pid]->process_tid == tid) { // Kill process (TID == process TID)

		uint64_t threads_count = process_table[pid]->threads.size();
		uint64_t i = 0;
		std::map<kiv_os::THandle, std::unique_ptr<Thread>>::iterator thread_it = process_table[pid]->threads.begin();

		while (thread_it != process_table[pid]->threads.end()) {
			if (process_table[pid]->process_tid != thread_it->second->tid) {
				Set_Free_TID(thread_it->second->tid);

				stdtid_to_tid.erase(thread_it->second->std_tid);
				tid_to_pid.erase(thread_it->second->tid);
				process_table[pid]->threads.erase(thread_it->second->tid);
			}

			i++;
			if (i >= threads_count) {
				break;
			}

			thread_it++;
		}

		Set_Free_PID(pid);
		Set_Free_TID(tid);

		stdtid_to_tid.erase(stdtid);
		tid_to_pid.erase(tid);
		process_table[pid]->threads.erase(tid);
		process_table.erase(pid);

	} else { // Kill thread (TID != process TID)
		Set_Free_TID(tid);

		stdtid_to_tid.erase(stdtid);
		tid_to_pid.erase(tid);
		process_table[pid]->threads.erase(tid);
	}

	regs.rcx.x = exit_code;
}

void Process_Controller::Exit(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock(process_table_mutex);

	uint16_t exit_code = static_cast<decltype(regs.rcx.x)>(regs.rcx.x);

	if (process_table.size() == 0) {
		return;
	}

	kiv_os::THandle tid = Get_TID();

	if (tid_to_pid.find(tid) == tid_to_pid.end()) {
		return;
	}

	kiv_os::THandle pid = tid_to_pid[tid];

	if (process_table.find(pid) == process_table.end()) {
		return;
	}

	if (process_table[pid]->threads.find(tid) == process_table[pid]->threads.end()) {
		return;
	}

	if (process_table[pid]->process_tid == tid) { // Kill process (TID == process TID)

		uint64_t threads_count = process_table[pid]->threads.size();
		uint64_t i = 0;
		std::map<kiv_os::THandle, std::unique_ptr<Thread>>::iterator thread_it = process_table[pid]->threads.begin();
		while (thread_it != process_table[pid]->threads.end()) {
			if (process_table[pid]->process_tid != thread_it->second->tid) {
				thread_it->second->waiting_handles.clear();
				Notify_All(thread_it->second->tid);
				process_table[pid]->Join_thread(thread_it->second->tid, exit_code);
			}

			i++;
			if (i >= threads_count) {
				break;
			}

			thread_it++;
		}


		process_table[pid]->threads[tid]->waiting_handles.clear();
		Notify_All(tid);
		process_table[pid]->Join_thread(tid, exit_code);

	} else { // Kill thread (TID != process TID)
		process_table[pid]->threads[tid]->waiting_handles.clear();
		Notify_All(tid);
		process_table[pid]->Join_thread(tid, exit_code);
	}
}

void Process_Controller::Shutdown(kiv_hal::TRegisters& regs) {
	std::unique_lock<std::mutex> lock_mutex(process_table_mutex);

	sig_shutdown = true;

	for (int i = (int)(process_table.size() - 1); i >= 0; i--) {
  		uint64_t threads_count = process_table[i]->threads.size();
		uint64_t j = 0;
		std::map<kiv_os::THandle, std::unique_ptr<Thread>>::iterator thread_it = process_table[i]->threads.begin();
		while (thread_it != process_table[i]->threads.end()) {
			kiv_hal::TRegisters terminate_regs;

			uint64_t term_handle_count = thread_it->second->terminate_handles.size();
			uint64_t k = 0;
			std::map<kiv_os::NSignal_Id, kiv_os::TThread_Proc>::iterator term_handle_it = thread_it->second->terminate_handles.begin();
			while (term_handle_it != thread_it->second->terminate_handles.end()) {
				term_handle_it->second(terminate_regs);

				k++;
				if (k >= term_handle_count) {
					break;
				}

				term_handle_it++;
			}

			if (process_table[i]->process_tid != thread_it->second->tid || (strcmp(process_table[i]->process_name.c_str(), "shell") == 0 && process_table[i]->pid > 1)) {
				thread_it->second->waiting_handles.clear();
				process_table[i]->Join_thread(thread_it->second->tid, 0);
				Notify_All(thread_it->second->tid);
			}

			j++;
			if (j >= threads_count) {
				break;
			}

			thread_it++;
		}
	}

	lock_mutex.unlock();
}

void Process_Controller::Register_Signal_Handler(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock(process_table_mutex);

	kiv_os::NSignal_Id signal = static_cast<kiv_os::NSignal_Id>(regs.rcx.r);
	kiv_os::TThread_Proc process_handle = reinterpret_cast<kiv_os::TThread_Proc>(regs.rdx.r);

	kiv_os::THandle tid = Get_TID();

	if (tid_to_pid.find(tid) == tid_to_pid.end()) {
		return;
	}

	kiv_os::THandle pid = tid_to_pid[tid];

	if (process_table.find(pid) == process_table.end()) {
		return;
	}

	if (process_table[pid]->threads.find(tid) == process_table[pid]->threads.end()) {
		return;
	}

	if (sig_shutdown) {
		kiv_hal::TRegisters terminate_regs;
		process_handle(terminate_regs);
		return;
	}

	process_table[pid]->threads[tid]->terminate_handles.insert(std::pair<kiv_os::NSignal_Id, kiv_os::TThread_Proc>(signal, process_handle));
}

void Process_Controller::Handle_Process(kiv_hal::TRegisters& regs) {
	switch (static_cast<kiv_os::NOS_Process>(regs.rax.l)) {
		case kiv_os::NOS_Process::Clone:
			Clone(regs);
			break;
		case kiv_os::NOS_Process::Wait_For:
			Wait_For(regs);
			break;
		case kiv_os::NOS_Process::Read_Exit_Code:
			Read_Exit_Code(regs);
			break;
		case kiv_os::NOS_Process::Exit:
			Exit(regs);
			break;
		case kiv_os::NOS_Process::Shutdown:
			Shutdown(regs);
			break;
		case kiv_os::NOS_Process::Register_Signal_Handler:
			Register_Signal_Handler(regs);
			break;
	}
}

void Process_Controller::Notify_All(kiv_os::THandle tid) {
	if (tid_to_pid.find(tid) == tid_to_pid.end()) {
		return;
	}

	kiv_os::THandle pid = tid_to_pid[tid];

	if (process_table.find(pid) == process_table.end()) {
		return;
	}

	if (process_table[pid]->threads.find(tid) == process_table[pid]->threads.end()) {
		return;
	}

	if (process_table[pid]->threads[tid]->sleeped_handles.size() > 0)	{

		uint64_t sleeped_tid_count = process_table[pid]->threads[tid]->sleeped_handles.size();
		uint64_t i = 0;
		std::map<kiv_os::THandle, kiv_os::THandle>::iterator sleeped_tid_it = process_table[pid]->threads[tid]->sleeped_handles.begin();
		while (sleeped_tid_it != process_table[pid]->threads[tid]->sleeped_handles.end()) {
			Notify(sleeped_tid_it->second, tid);

			i++;
			if (i >= sleeped_tid_count) {
				break;
			}

			sleeped_tid_it++;
		}

		process_table[pid]->threads[tid]->sleeped_handles.clear();
	}
}

void Process_Controller::Notify(kiv_os::THandle sleeped_tid, kiv_os::THandle waiting_tid) {
	kiv_os::THandle pid = tid_to_pid[sleeped_tid];
	
	process_table[pid]->threads[sleeped_tid]->Restart(waiting_tid);
}

kiv_os::THandle Process_Controller::Get_Free_PID() {
	std::lock_guard<std::mutex> lock(used_pid_mutex);

	kiv_os::THandle pid = free_pid;
	uint64_t counter = 0;

	while (used_pid[pid] != false && pid < MAX_PID_COUNT) {
		if (counter >= MAX_PID_COUNT) {
			pid = -1;
			free_pid = 0;
			return pid;
		}

		pid = (pid + 1) % MAX_PID_COUNT;
		counter++;
	}

	free_pid = (pid + 1) % MAX_PID_COUNT;

	if (pid >= MAX_PID_COUNT) pid = MIN_PID;
	used_pid[pid] = true;

	return pid;
}

void Process_Controller::Set_Free_PID(kiv_os::THandle pid) {
	std::lock_guard<std::mutex> lock(used_pid_mutex);

	used_pid[pid] = false;
	
	if (pid < free_pid) {
		free_pid = pid;
	}
}

kiv_os::THandle Process_Controller::Get_Free_TID() {
	std::lock_guard<std::mutex> lock(used_tid_mutex);

	kiv_os::THandle tid = free_tid;
	uint64_t counter = 0;

	while (used_tid[tid - MIN_TID] != false && tid < MIN_TID + MAX_TID_COUNT) {
		if (counter >= MAX_TID_COUNT)	{
			tid = -1;
			free_tid = 0;
			return tid;
		}

		tid = ((tid + 1) % MAX_TID_COUNT) + MIN_TID;
		counter++;
	}

	free_tid = ((tid + 1) % MAX_TID_COUNT) + MIN_TID;
	
	if (tid >= MAX_TID_COUNT + MIN_TID) tid = MIN_TID;
	used_tid[tid - MIN_TID] = true;

	return tid;
}

void Process_Controller::Set_Free_TID(kiv_os::THandle tid) {
	std::lock_guard<std::mutex> lock(used_tid_mutex);

	used_tid[tid - MIN_TID] = false;

	if (tid < free_tid) {
		free_tid = tid;
	}
}

uint64_t Process_Controller::Get_STD_TID(kiv_os::THandle tid) {
	std::map<uint64_t, kiv_os::THandle>::iterator it = stdtid_to_tid.begin();

	while (it != stdtid_to_tid.end()) {
		if (it->second == tid) {
			return it->first;
		}

		it++;
	}

	return 0;
}

kiv_os::THandle Process_Controller::Exist_TID(kiv_os::THandle tid) {
	if (tid_to_pid.find(tid) == tid_to_pid.end()) {
		return 0;
	}

	return tid;
}

kiv_os::THandle Process_Controller::Get_PID() {
	kiv_os::THandle tid = Get_TID();
	return tid_to_pid.find(tid)->second;
}

kiv_os::THandle Process_Controller::Get_TID() {
	uint64_t stdtid = std::hash<std::thread::id>()(std::this_thread::get_id());
	return stdtid_to_tid[stdtid];
}
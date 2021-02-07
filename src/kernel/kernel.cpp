#pragma once

#include "kernel.h"

HMODULE User_Programs;

kiv_os::THandle shell_handle_in;
kiv_os::THandle shell_handle_out;

std::unique_ptr<IO> io;

void Initialize_Kernel() {
	User_Programs = LoadLibraryW(L"user.dll");
}

void Shutdown_Kernel() {
	FreeLibrary(User_Programs);
}

kiv_hal::TRegisters Prepare_SysCall_Context(kiv_os::NOS_Service_Major major, uint8_t minor)
{
	kiv_hal::TRegisters regs;
	regs.rax.h = static_cast<uint8_t>(major);
	regs.rax.l = minor;
	return regs;
}

kiv_os::THandle Create_Kernel_Process() {
	std::unique_ptr<Process> process = std::make_unique<Process>("kernel", io->process_controller->Get_Free_PID());
	std::unique_ptr<Thread> thread = std::make_unique<Thread>(io->process_controller->Get_Free_TID(), process->pid);
	thread->std_tid = Thread::Get_STD_TID(std::this_thread::get_id());

	uint64_t std_tid = thread->std_tid;
	kiv_os::THandle tid = thread->tid;
	kiv_os::THandle pid = process->pid;

	process->state = State::Running;
	process->process_tid = tid;
	process->working_dir = "/";
	process->working_dir_sector = 0;

	process->threads.insert(std::pair<kiv_os::THandle, std::unique_ptr<Thread>>(tid, std::move(thread)));

	io->process_controller->stdtid_to_tid.insert({ std_tid, tid });
	io->process_controller->tid_to_pid.insert({ tid, pid });
	io->process_controller->process_table.insert({ pid, std::move(process) });

	return tid;
}

void Remove_Kernel_Process(kiv_os::THandle kernel_handler) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Exit));
	regs.rcx.x = static_cast<decltype(regs.rcx.x)>(kiv_os::NOS_Error::Success);

	io->process_controller->Exit(regs);

	regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Read_Exit_Code));
	regs.rdx.x = kernel_handler;

	io->process_controller->Read_Exit_Code(regs);
}

kiv_os::THandle Shell_Clone() {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Clone));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>("shell");
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>("");
	regs.rbx.e = (shell_handle_in << 16) | shell_handle_out;

	io->process_controller->Create_Process(regs);
	return static_cast<kiv_os::THandle>(regs.rax.x);
}

void Shell_Wait(kiv_os::THandle shell_handle) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Wait_For));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(&shell_handle);
	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(1);

	io->process_controller->Handle_Process(regs);
}

void Shell_Close(kiv_os::THandle shell_handle)
{
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Close_Handle));

	regs.rdx.x = static_cast<decltype(regs.rdx.r)>(shell_handle_in);
	io->Handle_IO(regs);

	regs.rdx.x = static_cast<decltype(regs.rdx.r)>(shell_handle_out);
	io->Handle_IO(regs);

	regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Read_Exit_Code));
	regs.rdx.x = static_cast<decltype(regs.rdx.r)>(shell_handle);

	io->process_controller->Handle_Process(regs);
}

void Set_Error(const bool failed, kiv_hal::TRegisters &regs) {
	if (failed) {
		regs.flags.carry = true;
		regs.rax.r = GetLastError();
	} else {
		regs.flags.carry = false;
	}
}

void __stdcall Sys_Call(kiv_hal::TRegisters& regs) {
	switch (static_cast<kiv_os::NOS_Service_Major>(regs.rax.h)) {
		case kiv_os::NOS_Service_Major::File_System:
			io->Handle_IO(regs);
			break;
		case kiv_os::NOS_Service_Major::Process:
			io->process_controller->Handle_Process(regs);
			break;
	}
}

void __stdcall Bootstrap_Loader(kiv_hal::TRegisters& context) {
	Initialize_Kernel();
	kiv_hal::Set_Interrupt_Handler(kiv_os::System_Int_Number, Sys_Call);

	uint16_t bytes_per_sector = 0;
	uint64_t number_of_sectors = 0;

	kiv_hal::TRegisters regs;

	shell_handle_in = Convert_Native_Handle(new STD_Handle_In());
	shell_handle_out = Convert_Native_Handle(new STD_Handle_Out());

	for (regs.rdx.l = 0; ; regs.rdx.l++) {
		kiv_hal::TDrive_Parameters params;
		regs.rax.h = static_cast<uint8_t>(kiv_hal::NDisk_IO::Drive_Parameters);;
		regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(&params);
		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, regs);

		if (!regs.flags.carry) {
			bytes_per_sector = params.bytes_per_sector;
			number_of_sectors = params.absolute_number_of_sectors;

			const char dec_2_hex[16] = { L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'A', L'B', L'C', L'D', L'E', L'F' };
			char hexa[3];
			hexa[0] = dec_2_hex[regs.rdx.l >> 4];
			hexa[1] = dec_2_hex[regs.rdx.l & 0xf];
			hexa[2] = 0;

			io = std::make_unique<IO>(number_of_sectors, bytes_per_sector, regs.rdx.l);
			break;
		}

		if (regs.rdx.l == 255) {
			break;
		}
	}

	kiv_os::THandle kernel_handler = 0;
	kernel_handler = Create_Kernel_Process();

	kiv_os::THandle handle = Shell_Clone();
	Shell_Wait(handle);
	Shell_Close(handle);

 	Remove_Kernel_Process(kernel_handler);

	Shutdown_Kernel();
}
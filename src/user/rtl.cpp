#include "rtl.h"

std::atomic<kiv_os::NOS_Error> kiv_os_rtl::Last_Error;

kiv_hal::TRegisters Prepare_SysCall_Context(kiv_os::NOS_Service_Major major, uint8_t minor) {
	kiv_hal::TRegisters regs;
	regs.rax.h = static_cast<uint8_t>(major);
	regs.rax.l = minor;
	return regs;
}

void Default_Signal_Handler() {
	return;
}

bool kiv_os_rtl::Open_File(const char* filename, kiv_os::NOpen_File flags, kiv_os::NFile_Attributes attributes, kiv_os::THandle& open) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Open_File));

	// Put it into string (dunno why this works with string and not only with const char*).
	std::string str = std::string(filename, strlen(filename));
	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(str.data());
	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(flags);
	regs.rdi.r = static_cast<decltype(regs.rdi.r)>(attributes);

	bool syscall_result = kiv_os::Sys_Call(regs);
	open = regs.rax.x;
	return syscall_result;
}

bool kiv_os_rtl::Write_File(kiv_os::THandle file_handle, const char* buffer, uint64_t buffer_size, uint64_t& written) {

	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Write_File));
	regs.rdx.x = static_cast<decltype(regs.rdx.x)>(file_handle);
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(buffer);
	regs.rcx.r = buffer_size;

	//TODO Write_File: check if file is read only (NICE TO HAVE).

	const bool syscall_result = kiv_os::Sys_Call(regs);
	written = regs.rax.r;

	return syscall_result;
}

bool kiv_os_rtl::Read_File(kiv_os::THandle file_handle, const char* buffer, uint64_t buffer_size, uint64_t& read) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Read_File));
	regs.rdx.x = static_cast<decltype(regs.rdx.x)>(file_handle);
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(buffer);
	regs.rcx.r = buffer_size;

	const bool syscall_result = kiv_os::Sys_Call(regs);
	read = regs.rax.r;
	return syscall_result;
}

bool kiv_os_rtl::Seek(kiv_os::THandle file_handle, kiv_os::NFile_Seek seek_operation, kiv_os::NFile_Seek new_position, uint64_t& position) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Seek));

	regs.rdx.x = static_cast<decltype(regs.rdx.x)>(file_handle);
	regs.rdi.r = static_cast<decltype(regs.rdi.r)>(position);
	regs.rcx.h = static_cast<decltype(regs.rcx.h)>(seek_operation);

	if (kiv_os::NFile_Seek::Set_Position == seek_operation) {
		regs.rcx.l = static_cast<decltype(regs.rcx.l)>(new_position);
	}

	bool syscall_result = kiv_os::Sys_Call(regs);

	if (kiv_os::NFile_Seek::Get_Position == seek_operation) {
		position = regs.rax.r;
	}

	return syscall_result;
}

bool kiv_os_rtl::Close_Handle(kiv_os::THandle file_handle) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Close_Handle));

	regs.rdx.x = static_cast<decltype(regs.rdx.x)>(file_handle);

	bool syscall_result = kiv_os::Sys_Call(regs);
	return syscall_result;
}

bool kiv_os_rtl::Delete_File(const char* filename) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Delete_File));

	std::string str = std::string(filename, strlen(filename));
	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(str.data());

	bool syscall_result = kiv_os::Sys_Call(regs);
	return syscall_result;
}

bool kiv_os_rtl::Set_Working_Dir(const char* new_directory, bool& success) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Set_Working_Dir));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(new_directory);

	bool syscall_result = kiv_os::Sys_Call(regs);
	success = static_cast<bool>(regs.rax.x);
	return syscall_result;
}

bool kiv_os_rtl::Get_Working_Dir(char* path, uint64_t path_size, uint64_t& written) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Get_Working_Dir));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(path);
	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(path_size);

	bool syscall_result = kiv_os::Sys_Call(regs);
	written = regs.rax.r;
	return syscall_result;
}

bool kiv_os_rtl::Create_Pipe(kiv_os::THandle* pipe_handles) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Create_Pipe));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(pipe_handles);

	bool syscall_result = kiv_os::Sys_Call(regs);
	return syscall_result;
}

bool kiv_os_rtl::Create_Process(const char* process_name, const char* args, kiv_os::THandle std_handle_in, kiv_os::THandle std_handle_out, kiv_os::THandle& process) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Clone));

	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(kiv_os::NClone::Create_Process);
	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(process_name);
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(args);
	regs.rbx.e = (std_handle_in << 16) | std_handle_out;

	bool syscall_result = kiv_os::Sys_Call(regs);
	process = static_cast<kiv_os::THandle>(regs.rax.r);
	return syscall_result;
}

bool kiv_os_rtl::Create_Thread(void* export_name, void* args, kiv_os::THandle std_handle_in, kiv_os::THandle std_handle_out, kiv_os::THandle& process) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Clone));

	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(kiv_os::NClone::Create_Thread);
	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(export_name);
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(args);

	bool syscall_result = kiv_os::Sys_Call(regs);
	process = static_cast<kiv_os::THandle>(regs.rax.r);
	return syscall_result;
}

bool kiv_os_rtl::Wait_For(kiv_os::THandle process_handlers[], int process_handlers_count, kiv_os::THandle& signalized_handler) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Wait_For));

	regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(process_handlers);
	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(process_handlers_count);

	bool syscall_result = kiv_os::Sys_Call(regs);
	signalized_handler = static_cast<kiv_os::THandle>(regs.rax.x);
	return syscall_result;
}

bool kiv_os_rtl::Read_Exit_Code(kiv_os::THandle process_handle, uint16_t& exit_code) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Read_Exit_Code));

	regs.rdx.x = static_cast<decltype(regs.rdx.x)>(process_handle);

	bool syscall_result = kiv_os::Sys_Call(regs);
	exit_code = static_cast<uint64_t>(regs.rax.x);
	return syscall_result;
}

bool kiv_os_rtl::Exit(uint16_t exit_code, kiv_os::THandle handle_out) {
	kiv_hal::TRegisters regs_exit = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Exit));
	regs_exit.rcx.x = static_cast<decltype(regs_exit.rcx.x)>(exit_code);

	bool syscall_result = kiv_os::Sys_Call(regs_exit);
	
	if (handle_out > 0) {
		kiv_hal::TRegisters regs_handle = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::File_System, static_cast<uint8_t>(kiv_os::NOS_File_System::Close_Handle));
		regs_handle.rbx.x = static_cast<decltype(regs_handle.rbx.x)>(0xFFFF);
		regs_handle.rdx.x = static_cast<decltype(regs_handle.rdx.x)>(handle_out);
		kiv_os::Sys_Call(regs_handle);
	}

	return syscall_result;
}

bool kiv_os_rtl::Shutdown() {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Shutdown));

	bool syscall_result = kiv_os::Sys_Call(regs);
	return syscall_result;
}

bool kiv_os_rtl::Register_Signal_Handler(kiv_os::NSignal_Id signal_Id, kiv_os::TThread_Proc process_handle) {
	kiv_hal::TRegisters regs = Prepare_SysCall_Context(kiv_os::NOS_Service_Major::Process, static_cast<uint8_t>(kiv_os::NOS_Process::Register_Signal_Handler));

	regs.rcx.r = static_cast<decltype(regs.rcx.r)>(signal_Id);

	if (process_handle == 0) {
		regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(Default_Signal_Handler);
	} else {
		regs.rdx.r = reinterpret_cast<decltype(regs.rdx.r)>(process_handle);
	}

	bool syscall_result = kiv_os::Sys_Call(regs);
	return syscall_result;
}
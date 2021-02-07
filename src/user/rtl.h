#pragma once

#include "..\api\api.h"
#include <atomic>
#include <vector>
#include <string>

namespace kiv_os_rtl {

	extern std::atomic<kiv_os::NOS_Error> Last_Error;

	//NOS_File_System
	bool Open_File(const char* filename, kiv_os::NOpen_File flags, kiv_os::NFile_Attributes attributes, kiv_os::THandle& open);
	bool Write_File(kiv_os::THandle file_handle, const char* buffer, uint64_t buffer_size, uint64_t& written);
	bool Read_File(kiv_os::THandle file_handle, const char* buffer, uint64_t buffer_size, uint64_t& read);
	bool Seek(kiv_os::THandle file_handle, kiv_os::NFile_Seek seek_operation, kiv_os::NFile_Seek new_position, uint64_t& position);
	bool Close_Handle(kiv_os::THandle file_handle);
	bool Delete_File(const char* filename);
	bool Set_Working_Dir(const char* new_directory, bool& success);
	bool Get_Working_Dir(char* path, uint64_t path_size, uint64_t& written);
	bool Create_Pipe(kiv_os::THandle* pipe_handles);

	//NOS_Process
	bool Create_Process(const char* export_name, const char* arguments, kiv_os::THandle stdin_handle, kiv_os::THandle stdout_handle, kiv_os::THandle& process);
	bool Create_Thread(void* export_name, void* arguments, kiv_os::THandle stdin_handle, kiv_os::THandle stdout_handle, kiv_os::THandle& process);
	bool Wait_For(kiv_os::THandle process_handlers[], int process_handlers_count, kiv_os::THandle& signalized_handler);
	bool Read_Exit_Code(kiv_os::THandle process_handle, uint16_t& exit_code);
	bool Exit(uint16_t exit_code, kiv_os::THandle handle_out);
	bool Shutdown();
	bool Register_Signal_Handler(kiv_os::NSignal_Id signal_Id, kiv_os::TThread_Proc process_handle);
}
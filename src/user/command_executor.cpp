#include "command_executor.h"

void cd(std::string new_directory, kiv_os::THandle handle_out) {
	bool success = true;

	kiv_os_rtl::Set_Working_Dir(new_directory.c_str(), success);
	if (!success) {
		char* error_message = "The system cannot find the listed path.\n";
		uint64_t written = 0;
		kiv_os_rtl::Write_File(handle_out, error_message, strlen(error_message), written);
	}
}

namespace Command_Executor {
	void Execute_Commands(std::vector<Command_Parser::Command>& commands, kiv_os::THandle handle_in, kiv_os::THandle handle_out) {
		std::vector<kiv_os::THandle> handles;
		std::map<uint64_t, kiv_os::THandle> pipes_handle_in;
		std::map<uint64_t, kiv_os::THandle> pipes_handle_out;
		kiv_os::THandle handle_in_tmp = handle_in;
		kiv_os::THandle handle_out_tmp = handle_out;
		uint64_t position = 0;
		bool close_file_in = false;
		bool close_file_out = false;

		for each (Command_Parser::Command command in commands) {
			if (strcmp(command.base.c_str(), "cd") != 0 && 
				strcmp(command.base.c_str(), "type") != 0 &&
				strcmp(command.base.c_str(), "md") != 0 &&
				strcmp(command.base.c_str(), "rd") != 0 && 
				strcmp(command.base.c_str(), "dir") != 0 && 
				strcmp(command.base.c_str(), "@echo") != 0 &&
				strcmp(command.base.c_str(), "echo") != 0 &&
				strcmp(command.base.c_str(), "find") != 0 &&
				strcmp(command.base.c_str(), "sort") != 0 &&
				strcmp(command.base.c_str(), "rgen") != 0 &&
				strcmp(command.base.c_str(), "freq") != 0 &&
				strcmp(command.base.c_str(), "tasklist") != 0 &&
				strcmp(command.base.c_str(), "shutdown") != 0 &&
				strcmp(command.base.c_str(), "exit") != 0 && 
				strcmp(command.base.c_str(), "shell") != 0) {

				uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Unknown_Error);

				std::string output = "Unknown command.\n";
				uint64_t written = 0;
				kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);
				return;
			}
			else {
				handle_in_tmp = handle_in;
				handle_out_tmp = handle_out;

				if (strcmp(command.base.data(), "cd") == 0) {
					cd(command.parameters, handle_out);
					return;
				} else {
					kiv_os::THandle handle;
					kiv_os::THandle pipe_handles[2];

					if (command.is_std_in) {
						kiv_os_rtl::Open_File(command.filename.c_str(), kiv_os::NOpen_File::fmOpen_Always, kiv_os::NFile_Attributes::System_File, handle_in_tmp);
						close_file_in = true;
					}

					if (command.is_std_out) {
						kiv_os_rtl::Open_File(command.filename.c_str(), static_cast<kiv_os::NOpen_File>(0), kiv_os::NFile_Attributes::System_File, handle_out_tmp);
						close_file_out = true;
					}

					if (command.is_pipe) {
						kiv_os_rtl::Create_Pipe(pipe_handles);
						pipes_handle_in.insert(std::pair<uint64_t, kiv_os::THandle>(position, pipe_handles[0]));
						pipes_handle_out.insert(std::pair<uint64_t, kiv_os::THandle>(position, pipe_handles[1]));
						handle_out_tmp = pipe_handles[1];
					}

					if (position > 0 && pipes_handle_in.find(position - 1) != pipes_handle_in.end()) {
						handle_in_tmp = pipes_handle_in[position - 1];
					}

					if (strcmp(command.base.c_str(), "@echo") == 0) {
						command.base = "echo";
						command.parameters = "@" + command.parameters;
					}

					kiv_os_rtl::Create_Process(command.base.c_str(), command.parameters.c_str(), handle_in_tmp, handle_out_tmp, handle);
					handles.push_back(handle);
					position++;
				}
			}
		}

		kiv_os::THandle sig_handler;
		kiv_os_rtl::Wait_For(handles.data(), (int)handles.size(), sig_handler);

		for (uint64_t i = 0; i < handles.size(); i++) {
			uint16_t exit_code = 0;
			kiv_os_rtl::Read_Exit_Code(handles[i], exit_code);
		}

		for (uint64_t i = 0; i < pipes_handle_out.size(); i++) {
			kiv_os_rtl::Close_Handle(pipes_handle_out[i]);
		}

		for (uint64_t i = 0; i < pipes_handle_in.size(); i++) {
			kiv_os_rtl::Close_Handle(pipes_handle_in[i]);
		}

		if (close_file_in) {
			kiv_os_rtl::Close_Handle(handle_in_tmp);
		}

		if (close_file_out) {
			kiv_os_rtl::Close_Handle(handle_out_tmp);
		}

		pipes_handle_in.clear();
		pipes_handle_out.clear();
		handles.clear();
	}
}
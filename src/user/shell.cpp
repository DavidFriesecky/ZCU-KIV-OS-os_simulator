#include "shell.h"

void Signalize_Shutdown() {
	sig_shutdown = true;
}

size_t __stdcall shell(const kiv_hal::TRegisters &regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	kiv_os_rtl::Register_Signal_Handler(kiv_os::NSignal_Id::Terminate, reinterpret_cast<kiv_os::TThread_Proc>(Signalize_Shutdown));

	bool ctrl_z = false;

	const int buffer_size = 256;
	char buffer[buffer_size];
	uint64_t counter;

	kiv_os_rtl::Write_File(handle_out, new_line, strlen(new_line), counter);
	kiv_os_rtl::Write_File(handle_out, intro, strlen(intro), counter);
	kiv_os_rtl::Write_File(handle_out, new_line, strlen(new_line), counter);

	while (true) {
		if (ctrl_z) {
			ctrl_z = false;
		} else if (is_echo_enabled) {
			uint64_t i;
			char working_dir[PATH_MAX_SIZE];
			uint64_t working_dir_size = 0;

			kiv_os_rtl::Get_Working_Dir(working_dir, PATH_MAX_SIZE, working_dir_size);

			std::string working_dir_tmp = working_dir;
			while ((i = working_dir_tmp.find("/")) != std::string::npos) {
				working_dir_tmp[i] = '\\';
			}

			kiv_os_rtl::Write_File(handle_out, prompt, strlen(prompt), counter);
			kiv_os_rtl::Write_File(handle_out, working_dir_tmp.c_str(), working_dir_size, counter);
			kiv_os_rtl::Write_File(handle_out, beak, strlen(beak), counter);
		}
	
		memset(buffer, 0, buffer_size);
		if (bool success = kiv_os_rtl::Read_File(handle_in, buffer, buffer_size, counter)) {
			if (counter >= buffer_size) {
				counter = buffer_size - 1;
			} else if (counter == 0) {
				ctrl_z = true;
				continue;
			}

			buffer[counter - 1] = 0;
			
			kiv_os_rtl::Write_File(handle_out, new_line, strlen(new_line), counter);
			
			if (strcmp(buffer, "exit") == 0) {
				break;
			}

			std::vector<Command_Parser::Command> commands = Command_Parser::Parse_Commands(buffer);
			Command_Executor::Execute_Commands(commands, handle_in, handle_out);

			if (sig_shutdown == true) {
				break;
			}
		} else {
			break;
		}
	}

	kiv_os_rtl::Exit(0, handle_out);
	return 0;	
}
#include "rgen.h"

uint64_t Signalize_Terminate(const kiv_hal::TRegisters& regs) {
	sig_terminate = true;
	return 0;
}

uint64_t Signalize_EOF(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);

	const int buffer_size = 1;
	char buffer[buffer_size];
	uint64_t read = 1;

	while (read && !sig_terminate) {
		kiv_os_rtl::Read_File(handle_in, buffer, 1, read);

		if (buffer[0] == 0 || buffer[0] == 3 || buffer[0] == 4 || buffer[0] == 26) {
			break;
		}
	}

	sig_eof = true;

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, 0);
	return 0;
}

size_t __stdcall rgen(const kiv_hal::TRegisters& regs) {
	kiv_os::NSignal_Id signal = kiv_os::NSignal_Id::Terminate;
	kiv_os::TThread_Proc handler = reinterpret_cast<kiv_os::TThread_Proc>(Signalize_Terminate);
	kiv_os_rtl::Register_Signal_Handler(signal, handler);

	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	const char* arguments = reinterpret_cast<const char*>(regs.rdi.r);

	std::string output;
	uint64_t written;

	if (strlen(arguments) != 0) {
		output = "Invalid arguments.\n";
		kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Invalid_Argument);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	kiv_os::THandle handle;
	sig_eof = false;
	kiv_os_rtl::Create_Thread(Signalize_EOF, &sig_eof, handle_in, handle_out, handle);

	srand(static_cast<unsigned>(time(0)));

	while (!sig_eof && !sig_terminate) {
		uint8_t random_number = static_cast<uint8_t>(rand());
		output = (char)random_number;
		kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);
	}

	output = "\n";
	kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

	std::vector<kiv_os::THandle> handles;
	handles.push_back(handle);
	kiv_os::THandle signalized_handler;

	uint16_t exit_code;

	if (sig_terminate) {
		kiv_os_rtl::Wait_For(handles.data(), 1, signalized_handler);
		kiv_os_rtl::Read_Exit_Code(handle, exit_code);
	}

	exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return exit_code;
}
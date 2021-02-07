#include "echo.h"

size_t __stdcall echo(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle std_out = static_cast<kiv_os::THandle>(regs.rbx.x);
	const char* args = reinterpret_cast<const char*>(regs.rdi.r);

	std::string output = "";
	uint64_t written = 0;

	if (strlen(args) == 0) {
		if (is_echo_enabled) {
			output.append("ECHO is enabled.\n");
			kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
		}
		else {
			output.append("ECHO is disabled.\n");
			kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
		}
	}
	else if (strcmp(args, HELP) == 0 || strcmp(args, "@") == 0) {
		if (strcmp(args, "@") == 0) {
			output.append("ERROR: ");
		}
		output.append("Enable or disable ECHO command:\n");
		output.append("  @echo [on | off]\n");
		output.append("Display message:\n");
		output.append("  echo [MESSAGE]\n");
		output.append("Display current setting of ECHO command:\n");
		output.append("  echo\n");
		kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
	}
	else if (strcmp(args, ECHO_ENABLE) == 0) {
		is_echo_enabled = true;
	}
	else if (strcmp(args, ECHO_DISABLE) == 0) {
		is_echo_enabled = false;
	} else {
		output.append(args);
		output.append("\n");
		kiv_os_rtl::Write_File(std_out, output.c_str(), output.size(), written);
	}

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, std_out);
	return 0;
}
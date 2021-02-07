#include "freq.h"

size_t __stdcall freq(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	const char* args = reinterpret_cast<const char*>(regs.rdi.r);

	std::string output;
	uint64_t written;

	if (strlen(args) != 0) {
		output = "Invalid arguments.\n";
		kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Invalid_Argument);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	const int buffer_size_freq = 256;
	int buffer_freq[buffer_size_freq] = {0};
	char output_buffer_freq[buffer_size_freq];

	const int buffer_size = 1;
	char buffer[buffer_size];

	uint64_t read = 1;
	uint64_t i;

	while (read) {
		kiv_os_rtl::Read_File(handle_in, buffer, 1, read);

		for (i = 0; i < read; i++) {
			uint8_t j = (uint8_t)(buffer[i]);
			buffer_freq[j]++;
		}
	}

	int output_buffer_freq_size;

	for (i = 0; i < buffer_size_freq; i++) {
		if (buffer_freq[i] > 0) {
			output_buffer_freq_size = sprintf_s(output_buffer_freq, "0x%hhx : %d\n", (uint8_t) i, buffer_freq[i]);
			kiv_os_rtl::Write_File(handle_out, output_buffer_freq, output_buffer_freq_size, written);
		}
	}
	output = "\n";
	kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return 0;
}
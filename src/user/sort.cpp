#include "sort.h"

size_t __stdcall sort(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);
	const char* args = reinterpret_cast<const char*>(regs.rdi.r);
	
	std::string filename = std::string(args);

	std::string output = "";
	uint64_t written;
	
	uint64_t position = 0;
	kiv_os::THandle handle_in_tmp = handle_in;
	bool is_file = false;

	if (filename.size() >= 1) {
		kiv_os_rtl::Open_File(filename.c_str(), kiv_os::NOpen_File::fmOpen_Always, kiv_os::NFile_Attributes::System_File, handle_in_tmp);
		kiv_os_rtl::Seek(handle_in_tmp, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, position);

		is_file = true;
	}

	if (handle_in_tmp == static_cast<kiv_os::THandle>(-1)) {
		output = "File not found.\n";
		kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::File_Not_Found);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	const int buffer_size = 512;
	char buffer[buffer_size];

	std::string read = "";
	uint64_t read_length = 1;

	while (read_length) {
		kiv_os_rtl::Read_File(handle_in_tmp, buffer, buffer_size, read_length);
		if (buffer[0] == 0x04) {
			break;
		}

		read.append(buffer, 0, read_length);

		if (is_file) {
			position += read_length;
			kiv_os_rtl::Seek(handle_in_tmp, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, position);
		}
	}

	if (is_file) {
		kiv_os_rtl::Close_Handle(handle_in_tmp);
	}

	std::stringstream stream(read);
	std::vector<std::string> lines;
	std::string line;

	while (std::getline(stream, line)) {
		lines.push_back(line);
	}

	std::sort(lines.begin(), lines.end());
	
	for each(line in lines) {
		output.append(line + " ");
	}

	output.append("\n\n");
	kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

	int16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return 0;
}
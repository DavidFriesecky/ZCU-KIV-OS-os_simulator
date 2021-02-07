#include "find.h"

bool print_count = false;
bool print_inverse = false;

bool Parse_Arguments(const char* args, std::string& filename, std::string& find_str) {
	std::string arguments = args;
	uint64_t i;
	bool flag_param = false;
	bool flag_quot = false;
	uint64_t start_index;

	for (i = 0; i < arguments.size(); i++) {
		if (arguments[i] == '/') {
			flag_param = true;
			continue;
		} else if (arguments[i] == '\"') {
			flag_quot = true;
		} else if (arguments[i] == ' ') {
			continue;
		}

		if (flag_param) {
			if (arguments[i] == 'v') {
				print_inverse = true;
			} else if (arguments[i] == 'c') {
				print_count = true;
			} else {
				return false;
			}
			flag_param = false;
		} else if (flag_quot) {
			i++;
			start_index = i;
			while (i < arguments.size() && arguments[i] != '\"') i++;
			find_str = arguments.substr(start_index + 1, i - start_index);
			flag_quot = false;
		} else {
			start_index = i;
			while (i < arguments.size() && arguments[i] != '\"' && arguments[i] != ' ') i++;
			filename = arguments.substr(start_index, i - start_index);
		}
	}

	return true;
}

bool Process_File(std::string filename, std::string find_str, kiv_os::THandle handle_in, kiv_os::THandle handle_out) {
	std::string output = "";
	uint64_t written;
	uint64_t count = 0;

	if (filename.size()) {
		if (strcmp(filename.c_str(), "") != 0) {
			output = "---------- " + filename + "\n";
			kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);
		}
	}

	uint64_t index = 0;
	uint64_t current_index = 0;
	uint64_t line_index = 0;

	const uint64_t buffer_size = 512;
	char line[buffer_size] = { 0 };
	char buffer[buffer_size] = { 0 };
	uint64_t read;

	bool new_line = false;
	bool found = false;

	if (filename.size() > 0) {
		kiv_os_rtl::Seek(handle_in, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, index);
	}
	kiv_os_rtl::Read_File(handle_in, buffer, sizeof(buffer), read);

	while (read) {
		while (current_index < read) {
			if (new_line || line_index >= sizeof(line) - 1) {
				found = (std::string(line).find(find_str) != std::string::npos && !find_str.empty()) || (find_str.empty() && std::string(line).empty());
				if (print_inverse) {
					found = !found;
				}

				if (found) {
					count++;

					if (!print_count) {
						output = line;
						output.append("\n");
						kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);
					}
				}

				memset(line, 0, sizeof(line));
				line_index = 0;
				new_line = false;
			}

			if (static_cast<kiv_hal::NControl_Codes>(buffer[current_index]) == kiv_hal::NControl_Codes::CR || static_cast<kiv_hal::NControl_Codes>(buffer[current_index]) == kiv_hal::NControl_Codes::LF) {
				new_line = true;
			} else {
				line[line_index++] = buffer[current_index];
			}

			current_index++;

			if (static_cast<kiv_hal::NControl_Codes>(buffer[current_index]) == kiv_hal::NControl_Codes::CR || static_cast<kiv_hal::NControl_Codes>(buffer[current_index]) == kiv_hal::NControl_Codes::LF) {
				new_line = true;
			}
		}

		index += current_index;
		current_index = 0;

		memset(buffer, 0, sizeof(buffer));
		if (filename.size() > 0) {
			kiv_os_rtl::Seek(handle_in, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, index);
		}
		kiv_os_rtl::Read_File(handle_in, buffer, sizeof(buffer), read);
	}

	if (print_count) {
		output = ": " + std::to_string(count) + "\n";
		kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);
	}

	return true;
}

size_t __stdcall find(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);
	const char* args = reinterpret_cast<const char*>(regs.rdi.r);
	std::string filename = "";
	std::string find_str = "";

	std::string output = "";
	uint64_t written;

	print_count = false;
	print_inverse = false;

	if (strlen(args) == 0 || !Parse_Arguments(args, filename, find_str)) {
		output = "Invalid arguments.\n";
		kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Invalid_Argument);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	if (filename.size() == 0) {
		if (!Process_File(filename, find_str, handle_in, handle_out)) {
			output = "File not found.\n";
			kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);

			uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::File_Not_Found);
			kiv_os_rtl::Exit(exit_code, handle_out);
			return 0;
		}
	} else {
		kiv_os::THandle handle;
		kiv_os_rtl::Open_File(filename.c_str(), kiv_os::NOpen_File::fmOpen_Always, kiv_os::NFile_Attributes::System_File, handle);
		if (handle == kiv_os::Invalid_Handle) {
			output = "File not found.\n";
			kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);

			uint16_t exit_code = static_cast<uint16_t>(kiv_os::Invalid_Handle);
			kiv_os_rtl::Exit(exit_code, handle_out);
			return 0;
		}

		bool result = Process_File(filename, find_str, handle, handle_out);
		kiv_os_rtl::Close_Handle(handle);

		if (!result) {
			output = "File not found.\n";
			kiv_os_rtl::Write_File(handle_out, output.data(), output.size(), written);

			uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::File_Not_Found);
			kiv_os_rtl::Exit(exit_code, handle_out);
			return 0;
		}
	}

	int16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return 0;
}
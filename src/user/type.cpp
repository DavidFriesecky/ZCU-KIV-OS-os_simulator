#include "type.h"

bool Is_Type_End_Code(char c) {
	return c == 0x00 || c == 0x03 || c == 0x04 || c == 0x1A;
}

size_t __stdcall type(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);
	const char* args = reinterpret_cast<const char*>(regs.rdi.r);

	std::string output = "";

	kiv_os::THandle handle_in_tmp;

	const uint64_t buffer_size = 4096;
	char buffer[buffer_size];
	uint64_t position = 0;
	uint64_t read = 1;
	uint64_t written = 0;

	if (strlen(args) == 0) {
		uint64_t written_new_line;

		bool eof = false;
		bool result;

		while (!eof) {
			result = kiv_os_rtl::Read_File(handle_in, buffer, buffer_size, read);
			if (!result) {
				return false;
			} else if (read == 0) {
				break;
			}

			eof = Is_Type_End_Code(buffer[read - 1]);
			if (eof) {
				if (--read == 0) {
					break;
				}
			}

			kiv_os_rtl::Write_File(handle_out, "\n", 1, written_new_line);
			result = kiv_os_rtl::Write_File(handle_out, buffer, read, written);
			if (!result || written == 0 || written_new_line == 0) {
				return false;
			}
		}

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	kiv_os_rtl::Open_File(args, kiv_os::NOpen_File::fmOpen_Always, kiv_os::NFile_Attributes::System_File, handle_in_tmp);

	if (handle_in_tmp == static_cast<kiv_os::THandle>(-1)) {
		output = "File not found.\n";
		kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::File_Not_Found);
		kiv_os_rtl::Exit(exit_code, handle_out);
		return 0;
	}

	kiv_os_rtl::Seek(handle_in_tmp, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, position);

	while (read) {
		kiv_os_rtl::Read_File(handle_in_tmp, buffer, buffer_size, read);

		if (read > 0) {
			output.append(buffer, 0, read);
		}

		position += read;
		kiv_os_rtl::Seek(handle_in_tmp, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, position);
	}

	output.append("\n");
	kiv_os_rtl::Write_File(handle_out, output.c_str(), output.size(), written);

	kiv_os_rtl::Close_Handle(handle_in_tmp);

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return 0;
}
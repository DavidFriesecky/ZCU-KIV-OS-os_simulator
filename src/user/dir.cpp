#include "dir.h"

size_t __stdcall dir(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle std_in = static_cast<kiv_os::THandle>(regs.rax.x);
	const kiv_os::THandle std_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	const char* arguments = reinterpret_cast<const char*>(regs.rdi.r);

	uint64_t written;
	uint64_t read;

	const int buffer_size = 512;
	char buffer[buffer_size] = { 0 };

	char processing_dir_name[14] = { 0 };

	if (strcmp(arguments, "/S") != 0) {		
		memcpy(buffer, arguments, strlen(arguments));
	}

	uint64_t current_index = 0;
	uint64_t index = 0;
	int dir_count = 0;
	int file_count = 0;
	const int max_item_count = 20;
	bool is_end_dir = false;

	char entries[sizeof(processing_dir_name) + ((sizeof(kiv_os::TDir_Entry) + 1) * max_item_count)];
	kiv_os::THandle handle;
	std::string output = "";

	kiv_os_rtl::Open_File(buffer, kiv_os::NOpen_File::fmOpen_Always, kiv_os::NFile_Attributes::Directory, handle);
	if (handle == static_cast<kiv_os::THandle>(-1)) {
		uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::File_Not_Found);
		kiv_os_rtl::Exit(exit_code, std_out);
		return 0;
	}
	// Set seek of directory to 0.
	kiv_os_rtl::Seek(handle, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, index);
	kiv_os_rtl::Read_File(handle, entries, sizeof(entries), read);

	while (read) {
		if (strcmp(arguments, "/S") == 0) {
			memcpy(processing_dir_name, entries + 1, sizeof(processing_dir_name));
			std::string processing_dir_name_tmp = processing_dir_name;
			processing_dir_name_tmp.erase(std::remove_if(processing_dir_name_tmp.begin(), processing_dir_name_tmp.end(), ::isspace), processing_dir_name_tmp.end());
			if (processing_dir_name_tmp.data()[processing_dir_name_tmp.size() - 1] == '.') processing_dir_name_tmp.data()[processing_dir_name_tmp.size() - 1] = '\0';

			if (processing_dir_name_tmp.size() > 0 && is_end_dir) {
				output.append("\n");
				output.append(processing_dir_name_tmp);
				output.append("\n------------\n");

				kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
				output = "";
				is_end_dir = false;
			}
		}

		while (current_index < read / (sizeof(kiv_os::TDir_Entry) + 1)) {
			kiv_os::TDir_Entry* entry = reinterpret_cast<kiv_os::TDir_Entry*>(sizeof(processing_dir_name) + entries + (current_index * (sizeof(kiv_os::TDir_Entry) + 1)));
			if (entry->file_attributes == static_cast<uint16_t>(kiv_os::NFile_Attributes::Directory)) {
				output.append("<DIR>");
				dir_count++;
			} else {
				output.append("     ");
				file_count++;
			}
			output.append("\t");
			output.append(entry->file_name);
			output.append("\n");

			kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
			output = "";

			current_index++;
		}
		index += current_index;
		current_index = 0;

		if ((uint8_t) entries[0] == 0xff && strcmp(arguments, "/S") == 0) {
			index = 0;
			is_end_dir = true;
		}

		if ((uint8_t) entries[0] == 0xff && strcmp(arguments, "/S") != 0) {
			break;
		}

		kiv_os_rtl::Seek(handle, kiv_os::NFile_Seek::Set_Position, kiv_os::NFile_Seek::Beginning, index);
		kiv_os_rtl::Read_File(handle, entries, sizeof(entries), read);

		if ((uint8_t)entries[0] == 0xfe) {
			break;
		}
	}

	output.append("\n");
	output.append("File(s): ");
	output.append(std::to_string(file_count));
	output.append("\n");

	output.append("Dir(s): ");
	output.append(std::to_string(dir_count));
	output.append("\n");

	kiv_os_rtl::Write_File(std_out, output.data(), output.size(), written);
	kiv_os_rtl::Close_Handle(handle);

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, std_out);
	return 0;
}
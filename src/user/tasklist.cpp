#include "tasklist.h"

size_t __stdcall tasklist(const kiv_hal::TRegisters& regs) {
	const kiv_os::THandle handle_out = static_cast<kiv_os::THandle>(regs.rbx.x);

	kiv_os::NOpen_File flags = static_cast<kiv_os::NOpen_File>(0);
	kiv_os::NFile_Attributes attributes = static_cast<kiv_os::NFile_Attributes>(0);
	
	const int buffer_size = 512;
	char buffer[buffer_size];
	
	uint64_t read = 1;
	uint64_t written = 0;

	kiv_os::THandle handle;
	kiv_os_rtl::Open_File("procfs", flags, attributes, handle);

	kiv_os_rtl::Read_File(handle, buffer, buffer_size, read);

	kiv_os_rtl::Write_File(handle_out, buffer, read, written);
	kiv_os_rtl::Write_File(handle_out, "\n", 1, written);

	kiv_os_rtl::Close_Handle(handle);

	uint16_t exit_code = static_cast<uint16_t>(kiv_os::NOS_Error::Success);
	kiv_os_rtl::Exit(exit_code, handle_out);
	return 0;
}
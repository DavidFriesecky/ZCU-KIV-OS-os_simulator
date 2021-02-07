#include "io.h"

std::mutex working_directory_mutex;
std::mutex io_mutex;

IO::IO(uint64_t number_of_sectors, uint16_t bytes_per_sector, uint8_t drive_id) {
	process_controller = std::make_unique<Process_Controller>();
	process_controller->sig_shutdown = false;
	
	fat12 = std::make_unique<FAT12>(drive_id);
	fat12->Init_FAT12(fat12, number_of_sectors, bytes_per_sector);
}

void IO::Handle_IO(kiv_hal::TRegisters& regs) {
	switch (static_cast<kiv_os::NOS_File_System>(regs.rax.l)) {
		case kiv_os::NOS_File_System::Open_File:
			Open_File(regs);
			break;
		case kiv_os::NOS_File_System::Write_File:
			Write_File(regs);
			break;
		case kiv_os::NOS_File_System::Read_File:
			Read_File(regs);
			break;
		case kiv_os::NOS_File_System::Seek:
			Seek(regs);
			break;
		case kiv_os::NOS_File_System::Close_Handle:
			Close_Handle(regs);
			break;
		case kiv_os::NOS_File_System::Delete_File:
			Delete_File(regs);
			break;
		case kiv_os::NOS_File_System::Set_Working_Dir:
			Set_Working_Dir(regs);
			break;
		case kiv_os::NOS_File_System::Get_Working_Dir:
			Get_Working_Dir(regs);
			break;
		case kiv_os::NOS_File_System::Create_Pipe:
			Create_Pipe(regs);
			break;
	}
}

void IO::Open_File(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock_mutex(io_mutex);
	std::lock_guard<std::mutex> lock_mutex_process(process_controller->process_table_mutex);

	char* filename = reinterpret_cast<char*>(regs.rdx.r);
	kiv_os::NOpen_File flags = static_cast<kiv_os::NOpen_File>(regs.rcx.r);
	kiv_os::NFile_Attributes attributes = static_cast<kiv_os::NFile_Attributes>(regs.rdi.r);
	
	kiv_os::THandle pid = process_controller->Get_PID();

	if (strlen(filename) > 0) {
		if (strcmp(filename, "procfs") == 0) {
			Procfs_Handle* handle = new Procfs_Handle();
			regs.rax.x = Convert_Native_Handle(static_cast<Procfs_Handle*>(handle));
			return;
		}
	}

	std::string returned_path = "";
	int returned_path_item_count = 0;
	bool founded = false;

	File_Entry* item = Functions::Get_Item(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, filename, returned_path, returned_path_item_count, founded);

	if (returned_path_item_count == ERROR_FIND) {
		Write_To_Console("Path does not exists.\n");
		regs.rax.x = -1;
		return;
	} 

	std::string parent_path;
	int k = founded ? 1 : 0;
	bool founded_parent = false;
	for (int i = 0, j = 0; i < returned_path.size(); i++) {
		parent_path.append(returned_path.c_str(), i, 1);

		if (returned_path.c_str()[i] == '/') {
			j++;
		}

		if (j > returned_path_item_count - k) {
			break;
		}
	}

	uint16_t parent_dir_sector = Functions::Get_Item(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, parent_path, returned_path, returned_path_item_count, founded_parent)->start_sector;

	File_Attribute* item_attribute = Functions::Read_Attribute(item->attribute);

	if (flags == kiv_os::NOpen_File::fmOpen_Always) {
		if (!founded) {
			Write_To_Console("File does not exists.\n");
			regs.rax.x = -1;
			return;
		}

		if (attributes == kiv_os::NFile_Attributes::Directory && item_attribute->Directory)	{
			Directory_Handle* dir_handle = new Directory_Handle();
			dir_handle->item = item;
			dir_handle->parent_dir_sector = parent_dir_sector;
			regs.rax.x = Convert_Native_Handle(static_cast<Item_Handle*>(dir_handle));
		} else if (attributes == kiv_os::NFile_Attributes::System_File && item_attribute->System_file) {
			File_Handle* file_handle = new File_Handle();
			file_handle->item = item;
			file_handle->parent_dir_sector = parent_dir_sector;
			regs.rax.x = Convert_Native_Handle(static_cast<Item_Handle*>(file_handle));
		} else if (attributes == kiv_os::NFile_Attributes::Directory && item_attribute->System_file) {
			Write_To_Console("Expected directory but item is a file.\n");
			regs.rax.x = -1;
			return;
		} else {
			Write_To_Console("Expected file but item is a directory.\n");
			regs.rax.x = -1;
			return;
		}
	} else {
		if (founded && attributes == kiv_os::NFile_Attributes::System_File) {
			File_Handle* file_handle = new File_Handle();
			file_handle->item = item;
			file_handle->parent_dir_sector = parent_dir_sector;
			regs.rax.x = Convert_Native_Handle(static_cast<Item_Handle*>(file_handle));
			return;
		}

		uint8_t attribute = 0;

		if (attributes == kiv_os::NFile_Attributes::Read_Only) attribute += (uint8_t)kiv_os::NFile_Attributes::Read_Only;
		if (attributes == kiv_os::NFile_Attributes::Hidden) attribute += (uint8_t)kiv_os::NFile_Attributes::Hidden;
		if (attributes == kiv_os::NFile_Attributes::System_File) attribute += (uint8_t)kiv_os::NFile_Attributes::System_File;
		if (attributes == kiv_os::NFile_Attributes::Volume_ID) attribute += (uint8_t)kiv_os::NFile_Attributes::Volume_ID;
		if (attributes == kiv_os::NFile_Attributes::Directory) attribute += (uint8_t)kiv_os::NFile_Attributes::Directory;
		if (attributes == kiv_os::NFile_Attributes::Archive) attribute += (uint8_t)kiv_os::NFile_Attributes::Archive;

		bool success = Functions::Create_Item(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, filename, attribute);

		if (!success) {
			Write_To_Console("Path not found or item already exists.\n");
		}

		File_Entry* new_item = Functions::Get_Item(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, filename, returned_path, returned_path_item_count, founded);

		if (attributes == kiv_os::NFile_Attributes::Directory) {
			Directory_Handle* dir_handle = new Directory_Handle();
			dir_handle->item = new_item;
			dir_handle->parent_dir_sector = parent_dir_sector;
			regs.rax.x = Convert_Native_Handle(static_cast<Item_Handle*>(dir_handle));
		} else {
			File_Handle* file_handle = new File_Handle();
			file_handle->item = new_item;
			file_handle->parent_dir_sector = parent_dir_sector;
			regs.rax.x = Convert_Native_Handle(static_cast<Item_Handle*>(file_handle));
		}
	}

	return;
}

void IO::Write_File(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock_mutex(io_mutex);

	auto file_handle = static_cast<IO_Handle*>(Resolve_kiv_os_Handle(regs.rdx.x));
	char* buffer = reinterpret_cast<char*>(regs.rdi.r);
	uint64_t buffer_length = regs.rcx.r;

	if (file_handle == INVALID_HANDLE_VALUE) {
		return;
	}

	regs.rax.r = file_handle->Write(fat12, process_controller, buffer, buffer_length);
}

void IO::Read_File(kiv_hal::TRegisters& regs) {
	auto file_handle = static_cast<IO_Handle*>(Resolve_kiv_os_Handle(regs.rdx.x));
	char* buffer = reinterpret_cast<char*>(regs.rdi.r);
	uint64_t buffer_length = regs.rcx.r;

	if (file_handle == INVALID_HANDLE_VALUE) {
		return;
	}

	regs.rax.r = file_handle->Read(fat12, process_controller, buffer, buffer_length);
}

void IO::Seek(kiv_hal::TRegisters& regs) {
	auto file_handle = static_cast<Item_Handle*>(Resolve_kiv_os_Handle(regs.rdx.x));
	kiv_os::NFile_Seek seek_operation = static_cast<kiv_os::NFile_Seek>(regs.rcx.h);
	kiv_os::NFile_Seek new_position = static_cast<kiv_os::NFile_Seek>(regs.rcx.l);
	uint64_t position = static_cast<uint64_t>(regs.rdi.r);

	if (file_handle == INVALID_HANDLE_VALUE) {
		return;
	}

	File_Entry* item = file_handle->item;
	uint64_t file_length = item->file_length;

	switch (seek_operation) {
		case kiv_os::NFile_Seek::Get_Position:
			regs.rax.r = file_handle->seek;
			return;
		case kiv_os::NFile_Seek::Set_Position:
			file_handle->Seek(new_position, position, file_length);
			return;
		case kiv_os::NFile_Seek::Set_Size:
			if (position == 0) {
				position = 1;
			}

			item->file_length = (uint32_t) position;


			uint64_t count = position / fat12->boot_sector->bytes_per_sector;
			if (position % fat12->boot_sector->bytes_per_sector != 0) {
				count++;
			}

			Functions::Remove_Item_Data(fat12, item->start_sector);
			Functions::Write_Item(fat12, NULL, item->start_sector, 0, count);
			FAT12::Save_FAT12_Table(fat12);
			return;
	}
}

void IO::Close_Handle(kiv_hal::TRegisters& regs) {
	auto handle = static_cast<IO_Handle*>(Resolve_kiv_os_Handle(regs.rdx.x));

	if (regs.rbx.x == 0xFFFF) {
		if (strcmp(handle->handle_name.c_str(), "Pipe_Handle_In") == 0) {
			handle->Close();
		}
		return;
	}

	if (handle != INVALID_HANDLE_VALUE) {
		handle->Close();
		Remove_Handle(regs.rdx.x);
	}
}

void IO::Delete_File(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock_mutex(io_mutex);
	std::lock_guard<std::mutex> lock_mutex_process(process_controller->process_table_mutex);

	char* filename = reinterpret_cast<char*>(regs.rdx.r);

	kiv_os::THandle pid = process_controller->Get_PID();
	Functions::Remove_Item(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, filename);
}

void IO::Set_Working_Dir(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock_mutex(working_directory_mutex);

	char* new_directory = reinterpret_cast<char*>(regs.rdx.r);

	kiv_os::THandle pid = process_controller->Get_PID();
	bool success = Functions::Move_To_Dir(fat12, process_controller->process_table[pid]->working_dir, process_controller->process_table[pid]->working_dir_sector, new_directory);

	regs.rax.x = static_cast<decltype(regs.rax.x)>(success);
}

void IO::Get_Working_Dir(kiv_hal::TRegisters& regs) {
	std::lock_guard<std::mutex> lock_mutex(working_directory_mutex);

	char* path = reinterpret_cast<char*>(regs.rdx.r);
	uint64_t path_size = static_cast<uint64_t>(regs.rcx.r);

	kiv_os::THandle pid = process_controller->Get_PID();
	std::string current_path = process_controller->process_table[pid]->working_dir;

	strcpy_s(path, path_size, current_path.c_str());

	regs.rax.r = current_path.size();
}

void IO::Create_Pipe(kiv_hal::TRegisters& regs) {
	kiv_os::THandle* pipe_handle = reinterpret_cast<kiv_os::THandle*>(regs.rdx.r);

	Pipe_Handle* pipe_handle_in = new Pipe_Handle(PIPE_BUFFER_SIZE);
	Pipe_Handle* pipe_handle_out = new Pipe_Handle(pipe_handle_in->pipe);

	*(pipe_handle) = Convert_Native_Handle(static_cast<HANDLE>(pipe_handle_in));
	*(pipe_handle + 1) = Convert_Native_Handle(static_cast<HANDLE>(pipe_handle_out));
}

void IO::Write_To_Console(const char* message) {
	std::string str = std::string(message);
	kiv_hal::TRegisters registers;

	registers.rax.h = static_cast<decltype(registers.rax.l)>(kiv_hal::NVGA_BIOS::Write_String);
	registers.rdx.r = reinterpret_cast<decltype(registers.rdx.r)>(message);
	registers.rcx.r = str.length();
	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);
}
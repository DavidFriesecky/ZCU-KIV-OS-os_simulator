#include "io_handles.h"

uint64_t IO_Handle::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	return 0;
}

uint64_t IO_Handle::Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	return 0;
}

void IO_Handle::Close() {
	return;
}

uint64_t IO_Handle::Seek(kiv_os::NFile_Seek new_position, uint64_t actual_position, uint64_t size) {
	switch (new_position) {
		case kiv_os::NFile_Seek::Beginning:
			seek = actual_position + 1;
			break;
		case kiv_os::NFile_Seek::Current:
			seek += actual_position + 1;
			break;
		case kiv_os::NFile_Seek::End:
			seek = size - actual_position;
			break;
	}

	if (seek < 1) {
		seek = 1;
	}

	return seek;
}

uint64_t STD_Handle_In::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	kiv_hal::TRegisters registers;

	uint64_t position = 0;
	while (position < buffer_length) {
		registers.rax.h = static_cast<decltype(registers.rax.l)>(kiv_hal::NKeyboard::Read_Char);

		kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Keyboard, registers);

		if (!registers.flags.non_zero) {
			break;	// Nic jsme neprecetli, 
		}
		// if rax.l == EOT > correct end
		// else > error

		char c = registers.rax.l;

		// Osetrime zname kody.
		switch (static_cast<kiv_hal::NControl_Codes>(c)) {
			case kiv_hal::NControl_Codes::BS: 
				if (position > 0) {
					position--;
				}

				registers.rax.h = static_cast<decltype(registers.rax.l)>(kiv_hal::NVGA_BIOS::Write_Control_Char);
				registers.rdx.l = c;
				kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);
				break;

			case kiv_hal::NControl_Codes::LF:
				break;

			case kiv_hal::NControl_Codes::NUL:
				return 0;

			case kiv_hal::NControl_Codes::CR:
				buffer[position] = '\n';

				registers.rax.h = static_cast<decltype(registers.rax.l)>(kiv_hal::NVGA_BIOS::Write_String);
				registers.rdx.r = reinterpret_cast<decltype(registers.rdx.r)>(&buffer[position]);
				registers.rcx.r = 1;

				kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);

				position++;
				return position;

			default:
				buffer[position] = c;
				position++;

				registers.rax.h = static_cast<decltype(registers.rax.l)>(kiv_hal::NVGA_BIOS::Write_String);
				registers.rdx.r = reinterpret_cast<decltype(registers.rdx.r)>(&c);
				registers.rcx.r = 1;

				kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);
				break;
		}
	}

	return position;
}

uint64_t STD_Handle_Out::Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	kiv_hal::TRegisters registers;

	registers.rax.h = static_cast<decltype(registers.rax.h)>(kiv_hal::NVGA_BIOS::Write_String);
	registers.rdx.r = reinterpret_cast<decltype(registers.rdx.r)>(buffer);
	registers.rcx.r = buffer_length;

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::VGA_BIOS, registers);

	return (uint64_t) registers.rcx.r;
}

Pipe_Handle::Pipe_Handle(Pipe* pipe) {
	this->pipe = pipe;
	this->handle_name = "Pipe_Handle_In";
	type = Pipe_Type::Write;
}

Pipe_Handle::Pipe_Handle(int buffer_size) {
	pipe = new Pipe(buffer_size);
	this->handle_name = "Pipe_Handle_Out";
	type = Pipe_Type::Read;
}

uint64_t Pipe_Handle::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	uint64_t read_count = pipe->Consume(buffer, buffer_length);
	return read_count;
}

uint64_t Pipe_Handle::Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	uint64_t written_count = pipe->Produce(buffer, buffer_length);
	return written_count;
}

void Pipe_Handle::Close() {
	pipe->Close(type);
}

uint64_t File_Handle::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	if ((seek - 1) >= item->file_length) {
		return 0;
	}

	uint64_t sectors_count = Functions::Get_Sectors_Count(fat12, item->start_sector);
	uint64_t seek_sector = Functions::Find_Sector(fat12, (seek - 1) / fat12->boot_sector->bytes_per_sector, item->start_sector);
	bool exist_next_sector = true;
	size_t item_current_sector_count = 0;
	size_t item_current_sector_position = 0;

	uint64_t position = 0;

	while (exist_next_sector) {
		exist_next_sector = false;

		if (position >= buffer_length) {
			break;
		}

		Sector* read_sector = new Sector();
		memset(read_sector, 0, sizeof(*read_sector));

		Functions::Read_Item(fat12, read_sector, (uint16_t) seek_sector, 0, 1);
		char* read_buffer = reinterpret_cast<char*>(read_sector->data);

		exist_next_sector = Functions::Next_Sector(fat12, &seek_sector, seek_sector);

		if (item->file_length < buffer_length) {
			memcpy(buffer + position, read_buffer, item->file_length);
			position += item->file_length;
			break;
		}

		if (position + fat12->boot_sector->bytes_per_sector > buffer_length) {
			uint64_t diff_position = position + fat12->boot_sector->bytes_per_sector - buffer_length;
			memcpy(buffer + position, read_buffer, diff_position);
			position += diff_position;
			break;
		}

		if (exist_next_sector) {
			memcpy(buffer + position, read_buffer, fat12->boot_sector->bytes_per_sector);
			position += fat12->boot_sector->bytes_per_sector;
		} else {
			uint64_t last_sector_bytes_count = item->file_length % fat12->boot_sector->bytes_per_sector;
			memcpy(buffer + position, read_buffer, last_sector_bytes_count);
			position += last_sector_bytes_count;
			break;
		}
	}

	return position;
}

uint64_t File_Handle::Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	seek = 1;

	Functions::Remove_Item_Data(fat12, item->start_sector);
	item->file_length = (uint32_t) buffer_length;

	uint64_t sectors_count = 1;
	sectors_count += buffer_length / fat12->boot_sector->bytes_per_sector;
	Sector* item_data = new Sector[sectors_count];
	memset(item_data, 0, sizeof(*item_data) * sectors_count);
	memcpy(item_data, buffer, buffer_length);

	Functions::Write_Item(fat12, item_data, item->start_sector, 0, sectors_count);

	uint64_t count = Functions::Get_Sectors_Count(fat12, parent_dir_sector);
	Sector_Dir* dir = new Sector_Dir[count];
	memset(dir, 0, sizeof(*dir) * count);
	Functions::Read_Item(fat12, (Sector*) dir, parent_dir_sector, 0, count);

	for (int i = 0; i < count; i++) {
		for (int j = 0; j < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); j++) {
			if (Functions::Compare_Filename(dir[i].entries[j], *item) == 0) {
				dir[i].entries[j].file_length = item->file_length;
				Functions::Write_Item(fat12, (Sector*) &dir[i], parent_dir_sector, i, 1);
				return buffer_length;
			}
		}
	}

	return buffer_length;
}

uint64_t Directory_Handle::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	uint16_t directory_sector = 0;

	memset(buffer, 0, buffer_length);

	if (bfs_queue.size() > 0) {
		directory_sector = bfs_queue[0].start_sector;
		memcpy(buffer + 1, bfs_queue[0].name, 8);
		buffer[9] = '.';
		memcpy(buffer + 10, bfs_queue[0].extension, 3);
		buffer[13] = '\0';
	} else if (item && item != nullptr) {
		directory_sector = item->start_sector;
	}

	if (first_sector == 0 && !loop_stop) {
		first_sector = directory_sector;
	}

	last_processed_sector = directory_sector;
	std::vector<File_Entry> directory_items = Functions::Get_Items(fat12, directory_sector);
	if (seek == 1) {
		for (uint64_t i = 0; i < directory_items.size(); i++) {
			File_Entry item_tmp = directory_items[i];
			if (Functions::Read_Attribute(item_tmp.attribute)->Directory && Functions::Compare_Filename(item_tmp, ".") != 0 && Functions::Compare_Filename(item_tmp, "..") != 0) {
				bfs_queue.push_back(item_tmp);
			}
		}
	}

	uint64_t position = 0;

	for (uint64_t i = (seek - 1); i < directory_items.size(); i++) {		
		if (position >= buffer_length - 14) {
			loop_stop = true;
			return position;
		}

		File_Entry item_tmp = directory_items[i];

		kiv_os::TDir_Entry entry;
		memset(&entry, 0, sizeof(kiv_os::TDir_Entry));

		entry.file_attributes = static_cast<uint16_t>(item_tmp.attribute);

		bool write_name = true;
		bool write_extension = false;
		uint64_t len = 0;
		for (uint64_t j = 0, k = 0; j < 12; j++, k++) {
			if (write_name && k < 8 && item_tmp.name[k] > 0x20) {
				entry.file_name[j] = item_tmp.name[k];
				continue;
			} else if (write_name) {
				write_name = false;
				write_extension = true;
				len += k;
				k = 0;
			}

			if (write_extension && k < 3 && item_tmp.extension[k] > 0x20) {
				if (k == 0) {
					len++;
					entry.file_name[j++] = '.';
				}
				entry.file_name[j] = item_tmp.extension[k];
				continue;
			} else {
				len += k;
				if (len < 11) entry.file_name[len] = '\0';
				break;
			}
		}

		memcpy(buffer + position + 14, &entry, sizeof(kiv_os::TDir_Entry));
		position += sizeof(kiv_os::TDir_Entry);
		memcpy(buffer + position + 14, "\0", 1);
		position++;
	}

	buffer[0] = (uint8_t)0xff;
	if (!bfs_queue.empty() && last_processed_sector == bfs_queue[0].start_sector) {
		bfs_queue.erase(bfs_queue.begin());
	}
	
	if (directory_sector == first_sector && loop_stop) {
		buffer[0] = (uint8_t)0xfe;
	}

	loop_stop = true;
	return position;
}

uint64_t Procfs_Handle::Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length) {
	uint64_t position = 0;
	std::stringstream stream;

	uint64_t process_count = process_controller->process_table.size();
	uint64_t i = 0;
	std::map<kiv_os::THandle, std::unique_ptr<Process>>::iterator process_it = process_controller->process_table.begin();
	stream << "Process name" << std::setw(10) << "\t" << "Process PID" << "\t" << "Process TID" << "\n";
	while (process_it != process_controller->process_table.end()) {
		stream << process_it->second->process_name.data() << std::setw(15) << "\t" << process_it->first << "\t\t" << process_it->second->process_tid << "\n";

		i++;
		if (i >= process_count) {
			break;
		}

		process_it++;
	}

	memcpy(buffer, stream.str().data(), (uint64_t) stream.tellp());

	return (uint64_t) stream.tellp();
}

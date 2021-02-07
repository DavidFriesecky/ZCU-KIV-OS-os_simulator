#pragma once

#include "fat12_header.h"
#include "Process_Controller.h"
#include "Pipe.h"
#include <sstream>
#include <iomanip>

class IO_Handle {
	public:
		uint64_t seek = 1;
		std::string handle_name = "IO_Handle";
		virtual uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
		virtual uint64_t Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
		virtual uint64_t Seek(kiv_os::NFile_Seek new_position, uint64_t actual_position, uint64_t size);
		virtual void Close();
};

class STD_Handle_In : public IO_Handle {
	public:
		uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
};

class STD_Handle_Out : public IO_Handle {
	public:
		uint64_t Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
};

class Pipe_Handle : public IO_Handle {
	public:
		Pipe* pipe;
		Pipe_Type type;

		Pipe_Handle(Pipe* pipe);
		Pipe_Handle(int buffer_size);

		uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
		uint64_t Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
		void Close();
};

class Item_Handle : public IO_Handle {
	public:
		uint16_t parent_dir_sector = 0;
		std::vector<File_Entry> bfs_queue;
		bool loop_stop = false;
		uint16_t first_sector = 0;
		uint16_t last_processed_sector = 0;
		File_Entry* item = nullptr;
};

class File_Handle : public Item_Handle {
public:
	uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
	uint64_t Write(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
};

class Directory_Handle : public Item_Handle {
	public:
		uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
};

class Procfs_Handle : public Item_Handle {
	public:
		uint64_t Read(std::unique_ptr<FAT12>& fat12, std::unique_ptr<Process_Controller>& process_controller, char* buffer, uint64_t buffer_length);
};
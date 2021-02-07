#pragma once

#include "..\api\api.h"
#include "handles.h"
#include "io_handles.h"
#include "process_controller.h"
#include "fat12_header.h"

#define PIPE_BUFFER_SIZE 4096

class IO {
	public:
		std::unique_ptr<FAT12> fat12;
		std::unique_ptr<Process_Controller> process_controller;

		IO(uint64_t number_of_sectors, uint16_t bytes_per_sector, uint8_t drive_id);
		void Handle_IO(kiv_hal::TRegisters& regs);

		void Open_File(kiv_hal::TRegisters& regs);
		void Write_File(kiv_hal::TRegisters& regs);
		void Read_File(kiv_hal::TRegisters& regs);
		void Seek(kiv_hal::TRegisters& regs);
		void Close_Handle(kiv_hal::TRegisters& regs);
		void Delete_File(kiv_hal::TRegisters& regs);
		void Set_Working_Dir(kiv_hal::TRegisters& regs);
		void Get_Working_Dir(kiv_hal::TRegisters& regs);
		void Create_Pipe(kiv_hal::TRegisters& regs);

		void Write_To_Console(const char* message);
};
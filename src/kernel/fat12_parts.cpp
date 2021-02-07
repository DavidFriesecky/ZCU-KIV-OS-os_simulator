#include "fat12_header.h"
#include <intrin.h>
FAT12::FAT12(uint8_t drive_id) {
	boot_sector = new Boot_Sector();
	table_phys = new FAT12_Table_Phys();
	table_logic = new FAT12_Table_Logic();
	this->drive_id = drive_id;
}

void FAT12::Load_FAT12_Boot_Sector(std::unique_ptr<FAT12>& fat12, uint64_t number_of_sectors, uint16_t bytes_per_sector) {
	Sector* sector_tmp = new Sector();
	Functions::Read_Sectors(fat12, 1, sector_tmp, BOOT_SECTOR);
	
	memcpy(&fat12->boot_sector->bytes_per_sector, sector_tmp->data + 11, 2);
	fat12->boot_sector->sectors_per_cluster = sector_tmp->data[13];
	fat12->boot_sector->fats_count = sector_tmp->data[16];
	memcpy(&fat12->boot_sector->root_directory_entries, sector_tmp->data + 17, 2);
	memcpy(&fat12->boot_sector->logical_sectors, sector_tmp->data + 19, 2);
	memcpy(&fat12->boot_sector->sectors_per_fat, sector_tmp->data + 22, 2);
}

void FAT12::Init_FAT12(std::unique_ptr<FAT12>& fat12, uint64_t number_of_sectors, uint16_t bytes_per_sector) {
	Load_FAT12_Boot_Sector(fat12, number_of_sectors, bytes_per_sector);
	Load_FAT12_Table(fat12);
}

void FAT12::Load_FAT12_Table(std::unique_ptr<FAT12>& fat12) {
	Functions::Read_Sectors(fat12, fat12->boot_sector->sectors_per_fat, reinterpret_cast<Sector*>(fat12->table_phys), FAT_TABLE_1_SECTOR);

	int count = fat12->boot_sector->bytes_per_sector * fat12->boot_sector->sectors_per_fat;

	for (int i = 0, j = 0; i < count; i += 3) {
		fat12->table_logic->data[j++] = (fat12->table_phys->data[i] + (fat12->table_phys->data[i + 1] << 8)) & 0x0FFF;
		fat12->table_logic->data[j++] = (fat12->table_phys->data[i + 1] + (fat12->table_phys->data[i + 2] << 8)) >> 4;
	}
}

void FAT12::Save_FAT12_Table(std::unique_ptr<FAT12>& fat12) {
	int i, j;

	for (i = 0, j = 0; j < FAT_TABLE_LOGIC_SIZE; j += 2) {
		fat12->table_phys->data[i++] = (uint8_t)(fat12->table_logic->data[j]);
		fat12->table_phys->data[i++] = (uint8_t)(((fat12->table_logic->data[j] >> 8) & (0x0F)) | ((fat12->table_logic->data[j + 1] << 4) & (0xF0)));
		fat12->table_phys->data[i++] = (uint8_t)(fat12->table_logic->data[j + 1] >> 4);
	}

	for (j = 0; j < fat12->boot_sector->fats_count; j++) {
		Functions::Write_Sectors(fat12, fat12->boot_sector->sectors_per_fat, reinterpret_cast<Sector*>(fat12->table_phys), FAT_TABLE_1_SECTOR);
	}
}

File_Attribute::File_Attribute() {
	Read_only = false;
	Hidden = false;
	System_file = false;
	Volume_ID = false;
	Directory = false;
	Archive = false;
	Reserved = 0;
}

Date::Date() {
	year = 0;
	month = 0;
	day = 0;
}

Time::Time() {
	hour = 0;
	minute = 0;
	second = 0;
}
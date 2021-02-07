#include "fat12_header.h"

void Push_Back(std::string& path, std::string c) {
	uint64_t i = 1;
	if (path[path.size() - 1] == '\0') i = 2;

	if (path[path.size() - i] != c[0]) {
		std::string tmp = path.substr(0, path.size() - (i - 1));
		tmp.append(c);
		path = tmp;
	}
}

uint16_t Find_Free_Sector(std::unique_ptr<FAT12>& fat12, int count) {
	uint16_t i = (fat12->boot_sector->root_directory_entries * sizeof(File_Entry)) / fat12->boot_sector->bytes_per_sector;
	int free_count = 0;

	if (count == 1) {
		for (i; i < FAT_TABLE_LOGIC_SIZE; i++) {
			if (fat12->table_logic->data[i] == 0) {
				return i;
			}
		}

		return 0;
	}

	for (i; i < FAT_TABLE_LOGIC_SIZE; i++) {
		if (fat12->table_logic->data[i] == 0) {
			free_count++;

			if (free_count == count) {
				return free_count;
			}
		}
	}

	return free_count;
}

int Functions::Compare_Filename(File_Entry item, std::string filename2) {
	char filename1_tmp[13] = { 0 };
	uint64_t j = 0;

	for (uint64_t i = 0; i < 8; i++) {
		if (item.name[i] > 0x20) {
			filename1_tmp[j] = item.name[i];
			j++;
		} else {
			break;
		}
	}

	for (uint64_t i = 0; i < 3; i++) {
		if (item.extension[i] > 0x20) {
			if (i == 0) filename1_tmp[j++] = '.';
			filename1_tmp[j] = item.extension[i];
			j++;
		} else {
			break;
		}
	}

	return strcmp(filename1_tmp, filename2.c_str());
}

int Functions::Compare_Filename(File_Entry item1, File_Entry item2) {
	int x;

	if ((x = strncmp(reinterpret_cast<const char*>(item1.name), reinterpret_cast<const char*>(item2.name), 8)) == 0) {
		if ((x = strncmp(reinterpret_cast<const char*>(item1.extension), reinterpret_cast<const char*>(item2.extension), 3)) == 0) {
			return 0;
		}
	}

	return x;
}

uint64_t Get_Path_Dir_Count(std::string path) {
	uint64_t count = 0;
	uint64_t i;

	if (path.size() <= 0) {
		return 0;
	}

	Push_Back(path, "/");

	std::string tok_tmp;
	std::string path_tmp = path;
	while ((i = path_tmp.find(FOLDER_SPLIT)) != std::string::npos) {
		tok_tmp = path_tmp.substr(0, i);
		path_tmp = path_tmp.substr(i + 1);

		if (i != 0 && strcmp(tok_tmp.c_str(), "..") != 0) count++;
	}

	return count;
}

bool Is_Directory_Empty(std::unique_ptr<FAT12>& fat12, File_Entry item) {
	if (!(Functions::Read_Attribute(item.attribute)->Directory)) {
		return true;
	}

	uint64_t count = Functions::Get_Sectors_Count(fat12, item.start_sector);
	Sector_Dir* dir = new Sector_Dir[count];
	memset(dir, 0, sizeof(*dir) * count);
	Functions::Read_Item(fat12, (Sector*) dir, item.start_sector, 0, count);

	for (int i = 0; i < count; i++) {
		for (int j = 0; j < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); j++) {
			if ((dir[i].entries[j].name[0] != FAT_DIRENT_NEVER_USED) && (dir[i].entries[j].name[0] != FAT_DIRENT_DELETED)) {
				return false;
			}
		}
	}

	return true;
}

Date* Int_To_Date(uint16_t date_int) {
	Date* date = new Date();
	date->year = (date_int / 512) + 1980;
	date->month = (date_int - ((date->year - 1980) * 512)) / 32;
	date->day = date_int - ((date->year - 1980) * 512) - (date->month * 32);
	return date;
}

Time* Int_To_Time(uint16_t time_int) {
	Time* time = new Time();
	time->hour = time_int / 2048;
	time->minute = (time_int - (time->hour * 2048)) / 32;
	time->second = (time_int - (time->hour * 2048) - (time->minute * 32)) * 2;
	return time;
}

uint16_t Sys_Date_To_Int(time_t sys_time) {
	uint16_t date = 0;
	tm ltm;
	localtime_s(&ltm, &sys_time);

	date += (ltm.tm_year - 80) * 512;
	date += (ltm.tm_mon + 1) * 32;
	date += ltm.tm_mday;

	return date;
}

uint16_t Sys_Time_To_Int(time_t sys_time) {
	uint16_t time = 0;
	tm ltm;
	localtime_s(&ltm, &sys_time);

	time += ltm.tm_hour * 2048;
	time += ltm.tm_min * 32;
	time += ltm.tm_sec >= 60 ? 29 : ltm.tm_sec / 2;

	return time;
}

bool Functions::Create_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path, uint8_t attribute) {
	std::string returned_path = "";
	int returned_path_item_count = 0;
	bool founded = false;
	File_Entry* item = Get_Item(fat12, working_dir, working_dir_sector, path, returned_path, returned_path_item_count, founded);
	std::string filename = "";
	File_Entry* parent_dir;
	std::string parent_path;
	uint16_t directory_sector_tmp = 0;

	uint64_t i, j;

	if (!founded && returned_path_item_count >= 0) {
		// Convert '\' to '/'.
		while ((i = path.find("\\")) != std::string::npos) {
			path.replace(i, 1, "/");
		}

		Push_Back(path, "/");
		
		for (i = path.size() - 2; i > 0; --i) {
			if (path.c_str()[i] == '/') {
				i++;
				break;
			}
		}

		filename = path.substr(i, path.size() - i - 1);

		if (filename.size() <= 0) return false;

		std::string unsupported_seq[] = { ".", "..", "\\", "\"", "<", ">", "?", "*", ":", "|" };
		for (i = 0; i < sizeof(unsupported_seq) / sizeof(std::string); i++) {
			if (strcmp(filename.c_str(), unsupported_seq[i].c_str()) == 0) return false;
		}

		parent_path = returned_path;
		if (strcmp(parent_path.c_str(), "/") != 0) {
			parent_dir = Get_Item(fat12, working_dir, working_dir_sector, parent_path, returned_path, returned_path_item_count, founded);
			directory_sector_tmp = parent_dir->start_sector;
		}

		i = filename.find(".");

		File_Entry new_item;
		if (i == std::string::npos) {
			if (filename.size() > 8) return false;
			for (j = 0; j < filename.size(); j++) new_item.name[j] = filename[j];
			for (j = filename.size(); j < 8; j++) new_item.name[j] = ' ';
			for (j = 0; j < 3; j++) new_item.extension[j] = ' ';
		} else {
			if (filename.substr(0, i).size() > 8 || filename.substr(i + 1).size() > 3) return false;
			for (j = 0; j < filename.substr(0, i).size(); j++) new_item.name[j] = filename[j];
			for (j = i; j < 8; j++) new_item.name[j] = ' ';
			for (j = 0; j < filename.substr(i + 1).size(); j++) new_item.extension[j] = filename[j + i + 1];
			for (j = filename.substr(i + 1).size(); j < 3; j++) new_item.name[j] = ' ';
		}
		
		new_item.attribute = attribute;
		memset(new_item.reserved, 0, sizeof(new_item.reserved));
		time_t timer;
		time(&timer);
		new_item.time = Sys_Time_To_Int(timer);
		new_item.date = Sys_Date_To_Int(timer);
		new_item.start_sector = Find_Free_Sector(fat12, 1);
		new_item.file_length = 0;

		Sector* new_sector = new Sector();
		memset(new_sector->data, 0, fat12->boot_sector->bytes_per_sector);
		Write_Item(fat12, new_sector, new_item.start_sector, 0, 1);

		fat12->table_logic->data[new_item.start_sector] = LAST_SECTOR;
		FAT12::Save_FAT12_Table(fat12);

		uint64_t count = Get_Sectors_Count(fat12, directory_sector_tmp);
		Sector_Dir* dir = new Sector_Dir[count];
		memset(dir, 0, sizeof(*dir) * count);
		Read_Item(fat12, (Sector*) dir, directory_sector_tmp, 0, count);

		for (i = 0; i < count; i++) {
			for (j = 0; j < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); j++) {
				if ((dir[i].entries[j].name[0] == FAT_DIRENT_NEVER_USED) || (dir[i].entries[j].name[0] == FAT_DIRENT_DELETED)) {
					dir[i].entries[j] = new_item;

					Write_Item(fat12, (Sector*) &dir[i], directory_sector_tmp, i, 1);

					return true;
				}
			}
		}

		Sector* new_dir_sector = new Sector();
		memset(new_dir_sector, 0, sizeof(*new_dir_sector));
		memcpy_s(new_dir_sector, sizeof(*new_dir_sector), &new_item, sizeof(File_Entry));
		uint16_t sector = Find_Free_Sector(fat12, 1);

		Write_Item(fat12, (Sector*) new_dir_sector, sector, std::string::npos, 1);

		fat12->table_logic->data[directory_sector_tmp] = sector;
		fat12->table_logic->data[sector] = LAST_SECTOR;
		FAT12::Save_FAT12_Table(fat12);

		return true;
	}

	printf("Item already exist or item on the path is file.\n");
	return false;
}

bool Functions::Remove_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path) {
	std::string returned_path = "";
	int returned_path_item_count = 0;
	bool founded = false;
	File_Entry* parent_dir;
	std::string parent_path = "";
	int i, j;

	if (path.size() <= 0) {
		return false;
	}

	File_Entry* item = Get_Item(fat12, working_dir, working_dir_sector, path, returned_path, returned_path_item_count, founded);

	if (!founded) {
		printf("Item does not exist.\n");
		return false;
	} 
	
	if (founded && returned_path_item_count == ROOT_DIR) {
		printf("Item is root directory.\n");
		return false;
	}

	if (Read_Attribute(item->attribute)->Directory && !Is_Directory_Empty(fat12, *item)) {
		printf("Directory is not empty.\n");
		return false;
	}

	for (i = 0, j = 0; i < returned_path.size(); i++) {
		parent_path.append(returned_path.c_str(), i, 1);

		if (returned_path.c_str()[i] == '/') {
			j++;
		}

		if (j > returned_path_item_count - 1) {
			break;
		}
	}

	parent_dir = Get_Item(fat12, working_dir, working_dir_sector, parent_path, returned_path, returned_path_item_count, founded);

	uint64_t count = Get_Sectors_Count(fat12, parent_dir->start_sector);
	Sector_Dir* dir = new Sector_Dir[count];
	memset(dir, 0, sizeof(*dir) * count);
	Read_Item(fat12, (Sector*) dir, parent_dir->start_sector, 0, count);

	for (i = 0; i < count; i++) {
		for (j = 0; j < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); j++) {
			if (Compare_Filename(dir[i].entries[j], *item) == 0) {
				dir[i].entries[j].name[0] = FAT_DIRENT_DELETED;
				Write_Item(fat12, (Sector*) &dir[i], parent_dir->start_sector, i, 1);

				Remove_Item_Data(fat12, item->start_sector);
				FAT12::Save_FAT12_Table(fat12);

				return true;
			}
		}
	}

	return false;
}

void Functions::Remove_Item_Data(std::unique_ptr<FAT12>& fat12, uint16_t start_sector) {
	uint64_t actual_sector = start_sector;
	uint64_t next_sector;

	while (Next_Sector(fat12, &next_sector, actual_sector)) {
		fat12->table_logic->data[actual_sector] = 0;
		actual_sector = next_sector;
	}
	fat12->table_logic->data[actual_sector] = 0;
}

bool Functions::Read_Item(std::unique_ptr<FAT12>& fat12, Sector* file, uint16_t start, uint64_t num, uint64_t count) {
	uint64_t temp;
	uint64_t dir_start = (uint64_t) fat12->boot_sector->fats_count * fat12->boot_sector->sectors_per_fat + 1;
	uint64_t data_start = dir_start + fat12->boot_sector->root_directory_entries * 32 / fat12->boot_sector->bytes_per_sector - 2;

	if (!start) {
		Functions::Read_Sectors(fat12, count, file, dir_start);
	} else {
		temp = Functions::Find_Sector(fat12, num, start);
		Functions::Read_Sectors(fat12, 1, file, data_start + temp);

		for (uint64_t i = 1; i < count; i++) {
			if (Functions::Next_Sector(fat12, &temp, temp)) {
				Functions::Read_Sectors(fat12, 1, file + i, data_start + temp);
			} else {
				break;
			}
		}
	}

	return true;
}

bool Functions::Write_Item(std::unique_ptr<FAT12>& fat12, Sector* file, uint16_t start, uint64_t num, uint64_t count) {


	uint64_t temp1, temp2;
	uint64_t dir_start = (uint64_t) fat12->boot_sector->fats_count * fat12->boot_sector->sectors_per_fat + 1;
	uint64_t data_start = dir_start + fat12->boot_sector->root_directory_entries * 32 / fat12->boot_sector->bytes_per_sector - 2;
	uint16_t i;

	if (start == 0) {
		for (i = 0; i < count; i++) {
			if (fat12->table_logic->data[i] == 0) {
				fat12->table_logic->data[i] = i + 1;
			}
		}
		fat12->table_logic->data[i] = LAST_SECTOR;
		Functions::Write_Sectors(fat12, count, file, dir_start);
	} else if (num != std::string::npos) {
		temp1 = Functions::Find_Sector(fat12, num, start);
		Functions::Write_Sectors(fat12, 1, file, data_start + temp1);

		for (i = 1; i < count; i++) {
			if (!Functions::Next_Sector(fat12, &temp1, temp1)) {
				temp2 = Find_Free_Sector(fat12, 1);
				fat12->table_logic->data[temp1] = (uint16_t) temp2;
				temp1 = temp2;				
			}
			Functions::Write_Sectors(fat12, 1, file + i, data_start + temp1);
		}

		fat12->table_logic->data[temp1] = LAST_SECTOR;
	} else {
		temp1 = start;
		Functions::Write_Sectors(fat12, 1, file, data_start + temp1);

		for (i = 1; i < count; i++) {
			temp2 = Find_Free_Sector(fat12, 1);
			fat12->table_logic->data[temp1] = (uint16_t) temp2;
			temp1 = temp2;

			Functions::Write_Sectors(fat12, 1, file + i, data_start + temp1);
		}

		fat12->table_logic->data[temp1] = LAST_SECTOR;
	}

	FAT12::Save_FAT12_Table(fat12);
	return true;
}

bool Functions::Move_To_Dir(std::unique_ptr<FAT12>& fat12, std::string& working_dir, uint16_t& working_dir_sector, std::string new_path) {
	std::string new_path_tmp = "";
	int new_path_dir_count_tmp = 0;
	bool founded = false;

	if (new_path.size() == 0) {
		for (uint64_t i = 0; working_dir[i] != '\0'; i++) {
			if (working_dir[i] != '/') {
				printf("%c", working_dir[i]);
			} else {
				printf("\\");
			}
			
		}
		printf("\n");
		return true;
	}

	new_path.push_back('\0');

	File_Entry* item = Get_Item(fat12, working_dir, working_dir_sector, new_path, new_path_tmp, new_path_dir_count_tmp, founded);

	if (!founded) {
		printf("Item does not exist or item on the path is file.\n");
		return false;
	}

	if (founded && new_path_dir_count_tmp == ROOT_DIR) {
		working_dir = "/";
		working_dir_sector = 0;
		return true;
	}

	if (!Read_Attribute(item->attribute)->Directory) {	
		printf("Item is not a directory.\n");
		return false;
	}

	working_dir = new_path_tmp;
	working_dir_sector = item->start_sector;
	return true;
}

File_Entry* Functions::Get_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path, std::string& return_path, int& return_path_item_count, bool& founded) {
	File_Entry* item = new File_Entry();

	uint64_t i, j, k;
	uint64_t move_back = 0;
	uint64_t count = 1;
	bool is_not_null = false;

	return_path.append("/");

	if (path.size() <= 0) {
		path = working_dir;
	}

	// Convert '\' to '/'.
	while ((i = path.find("\\")) != std::string::npos) {
		path.replace(i, 1, "/");
	}
	
	if (strcmp(path.c_str(), "/") == 0 || strcmp(path.c_str(), ".") == 0) {
		return_path_item_count = ROOT_DIR;
		founded = true;
		item->attribute = (uint8_t)kiv_os::NFile_Attributes::Directory;
		return item;
	}

	Push_Back(path, "/");

	uint16_t directory_tmp = working_dir_sector;
	std::string path_tmp = path;
	std::string tok_tmp;

	while ((i = path_tmp.find(FOLDER_SPLIT)) != std::string::npos) {
		tok_tmp = path_tmp.substr(0, i);
		path_tmp = path_tmp.substr(i + 1);

		if (strcmp(tok_tmp.c_str(), "..") == 0) {
			move_back++;
		}
	}

	uint64_t path_dir_count = Get_Path_Dir_Count(working_dir);
	uint64_t relative_path_dir_count = Get_Path_Dir_Count(path);

	if (path_dir_count < move_back) {
		return_path_item_count = ERROR_FIND;
		founded = false;
		return NULL;
	}

	if (path_dir_count - move_back == 0 && relative_path_dir_count == 0) {
		return_path_item_count = ROOT_DIR;
		founded = true;
		item->attribute = (uint8_t) kiv_os::NFile_Attributes::Directory;
		return item;
	}

	if (path.front() == '/') {
		directory_tmp = 0;
	} else if (move_back > 0) {
		directory_tmp = 0;

		path_tmp = working_dir;
		while ((i = path_tmp.find(FOLDER_SPLIT)) != std::string::npos) {
			if (i == 0) {
				path_tmp = path_tmp.substr(i + 1);
				continue;
			}

			if (path_dir_count - move_back > 0) {
				tok_tmp = path_tmp.substr(0, i);
				path_tmp = path_tmp.substr(i + 1);

				count = Get_Sectors_Count(fat12, directory_tmp);
				Sector_Dir* dir = new Sector_Dir[count];
				memset(dir, 0, sizeof(*dir) * count);
				Read_Item(fat12, (Sector*) dir, directory_tmp, 0, count);

				founded = false;

				for (j = 0; j < count; j++) {
					for (k = 0; k < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); k++) {
						if ((dir[j].entries[k].name[0] != FAT_DIRENT_NEVER_USED) && (dir[j].entries[k].name[0] != FAT_DIRENT_DELETED)) {
							if (Compare_Filename(dir[j].entries[k], tok_tmp) == 0) {
								*item = dir[j].entries[k];
								return_path.append(tok_tmp.c_str());
								return_path.append("/");
								return_path_item_count++;
								founded = true;
								directory_tmp = dir[j].entries[k].start_sector;
								break;
							}
						}
					}

					if (founded) {
						break;
					}
				}
			} else {
				break;
			}
			
			move_back++;
		}
	} else {
		return_path = working_dir;
		return_path_item_count = (int) Get_Path_Dir_Count(return_path);
	}

	path_tmp = path;
	while ((i = path_tmp.find(FOLDER_SPLIT)) != std::string::npos) {
		if (i == 0) {
			path_tmp = path_tmp.substr(i + 1);
			continue;
		}

		tok_tmp = path_tmp.substr(0, i);
		path_tmp = path_tmp.substr(i + 1);

		if (strcmp(tok_tmp.c_str(), "..") != 0) {
			if (!(Read_Attribute(item->attribute)->Directory) && founded) {
				return_path_item_count = ERROR_FIND;
				founded = false;
				return NULL;
			}

			count = Get_Sectors_Count(fat12, directory_tmp);
			Sector_Dir* dir = new Sector_Dir[count];
			memset(dir, 0, sizeof(*dir) * count);
			Read_Item(fat12, (Sector*) dir, directory_tmp, 0, count);

			founded = false;

			for (j = 0; j < count; j++) {
				for (k = 0; k < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); k++) {
					if ((dir[j].entries[k].name[0] != FAT_DIRENT_NEVER_USED) && (dir[j].entries[k].name[0] != FAT_DIRENT_DELETED)) {
						if (Compare_Filename(dir[j].entries[k], tok_tmp) == 0) {
							*item = dir[j].entries[k];
							return_path.append(tok_tmp.c_str());
							return_path.append("/");
							return_path_item_count++;
							founded = true;
							directory_tmp = dir[j].entries[k].start_sector;
							break;
						}
					}
				}

				if (founded) {
					break;
				}
			}

			if (!founded) {
				if (path_tmp.find(FOLDER_SPLIT) != std::string::npos) {
					return_path_item_count = ERROR_FIND;
					return NULL;
				}
			}
		}
	}

	return item;
}

std::vector<File_Entry> Functions::Get_Items(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path) {
	std::vector<File_Entry> items;

	std::string returned_path = "";
	int returned_path_item_count = 0;
	bool founded = false;
	File_Entry* item = Get_Item(fat12, working_dir, working_dir_sector, path, returned_path, returned_path_item_count, founded);
	uint16_t directory_sector = 0;

	if (!founded) {
		return items;
	}

	if (founded && returned_path_item_count == ROOT_DIR) {
		directory_sector = 0;
	} else {
		if (!(Read_Attribute(item->attribute)->Directory)) {
			return items;
		}

		directory_sector = item->start_sector;
	}

	items = Get_Items(fat12, directory_sector);

	return items;
}

std::vector<File_Entry> Functions::Get_Items(std::unique_ptr<FAT12>& fat12, uint16_t directory_sector) {
	std::vector<File_Entry> items;
	std::map<int, File_Entry> items_tmp;
	File_Entry item_tmp;
	int i, j, k;

	uint64_t count = Get_Sectors_Count(fat12, directory_sector);
	Sector_Dir* dir = new Sector_Dir[count];
	memset(dir, 0, sizeof(*dir) * count);
	Read_Item(fat12, (Sector*) dir, directory_sector, 0, count);

	k = 0;
	for (i = 0; i < count; i++) {
		for (j = 0; j < fat12->boot_sector->bytes_per_sector / sizeof(File_Entry); j++) {
			if ((dir[i].entries[j].name[0] != FAT_DIRENT_NEVER_USED) && (dir[i].entries[j].name[0] != FAT_DIRENT_DELETED)) {
				items_tmp.insert(std::pair<int, File_Entry>(k, dir[i].entries[j]));
				k++;
			}
		}
	}

	for (i = 0; i < items_tmp.size(); i++) {
		for (j = 0; j < items_tmp.size(); j++) {
			if (Compare_Filename(items_tmp[i], items_tmp[j]) < 0) {
				item_tmp = items_tmp[i];
				items_tmp[i] = items_tmp[j];
				items_tmp[j] = item_tmp;
			}
		}
	}

	for (i = 0; i < items_tmp.size(); i++) {
		items.push_back(items_tmp[i]);
	}

	return items;
}

uint64_t Functions::Get_Sectors_Count(std::unique_ptr<FAT12>& fat12, uint16_t start) {
	uint64_t count = 1, sector;

	if (start == 0) {
		return (fat12->boot_sector->root_directory_entries * sizeof(File_Entry)) / fat12->boot_sector->bytes_per_sector;
	}

	sector = start;
	while (Next_Sector(fat12, &sector, sector)) count++;

	return count;
}

uint16_t Functions::Find_Sector(std::unique_ptr<FAT12>& fat12, uint64_t num, uint16_t actual_sector) {
	uint64_t sector = actual_sector;

	for (int i = 0; i < num; i++) {
		Next_Sector(fat12, &sector, sector);
	}

	return (uint16_t) sector;
}

bool Functions::Next_Sector(std::unique_ptr<FAT12>& fat12, uint64_t* next_sector, uint64_t actual_sector) {
	*next_sector = fat12->table_logic->data[actual_sector];
	if (*next_sector == LAST_SECTOR || *next_sector == 0 || *next_sector >= 0x0FF0) {
		return false;
	} else {
		return true;
	}
}

File_Attribute* Functions::Read_Attribute(uint8_t attribute) {
	File_Attribute* attr = new File_Attribute();
	attr->Read_only = (attribute & (uint8_t)0x1) == ((uint8_t)kiv_os::NFile_Attributes::Read_Only);
	attr->Hidden = (attribute & (uint8_t)0x2) == ((uint8_t)kiv_os::NFile_Attributes::Hidden);
	attr->System_file = (attribute & (uint8_t)0x4) == ((uint8_t)kiv_os::NFile_Attributes::System_File);
	attr->Volume_ID = (attribute & (uint8_t)0x8) == ((uint8_t)kiv_os::NFile_Attributes::Volume_ID);
	attr->Directory = (attribute & (uint8_t)0x10) == ((uint8_t)kiv_os::NFile_Attributes::Directory);
	attr->Archive = (attribute & (uint8_t)0x20) == ((uint8_t)kiv_os::NFile_Attributes::Archive);
	attr->Reserved = attribute >> 6;

	return attr;
}

bool Functions::Read_Sectors(std::unique_ptr<FAT12>& fat12, uint64_t count, Sector* sector, uint64_t lba_index) {
	return Process_Sectors(kiv_hal::NDisk_IO::Read_Sectors, fat12->drive_id, count, sector, lba_index);
}

bool Functions::Write_Sectors(std::unique_ptr<FAT12>& fat12, uint64_t count, Sector* sector, uint64_t lba_index) {
	return Process_Sectors(kiv_hal::NDisk_IO::Write_Sectors, fat12->drive_id, count, sector, lba_index);
}

bool Functions::Process_Sectors(kiv_hal::NDisk_IO operation, uint8_t drive_id, uint64_t count, Sector* sector, uint64_t lba_index) {
	kiv_hal::TDisk_Address_Packet packet;

	packet.count = count;
	packet.sectors = reinterpret_cast<uint8_t*>(sector);
	packet.lba_index = lba_index;

	kiv_hal::TRegisters regs;
	regs.rdx.l = drive_id;
	regs.rax.h = static_cast<decltype(regs.rax.h)>(operation);
	regs.rdi.r = reinterpret_cast<decltype(regs.rdi.r)>(&packet);

	kiv_hal::Call_Interrupt_Handler(kiv_hal::NInterrupt::Disk_IO, regs);
	return !regs.flags.carry;
}
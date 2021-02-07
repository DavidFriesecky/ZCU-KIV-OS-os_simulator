#ifndef FAT12_HEADER_H
#define FAT12_HEADER_H
#include "../api/api.h"

#include <vector>
#include <iterator>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <map>

#define FAT_DIRENT_NEVER_USED 0x00
#define FAT_DIRENT_REALLY_0E5 0x05
#define FAT_DIRENT_DIRECTORY_ALIAS 0x2e
#define FAT_DIRENT_DELETED 0xe5

#define SECTOR_SIZE 512

#define BOOT_SECTOR 0
#define FAT_TABLE_1_SECTOR 1
#define FAT_TABLE_2_SECTOR 10

#define FAT_TABLE_SECTOR_COUNT 9
#define FAT_TABLE_LOGIC_SIZE 3072

#define EOF_FAT12 0xFF8
#define EOF_FAT16 0xFFF8
#define EOF_FAT32 0xFFFFFF8

#define MAX_PATH_SIZE 256

#define FOLDER_SPLIT '/'

#define LAST_SECTOR 0xFFF

#define ROOT_DIR 0
#define ERROR_FIND -1

// --- Structures ----------------------------------------------------- //
// Boot sector
struct Boot_Sector {
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint8_t fats_count;
    uint16_t root_directory_entries;
    uint16_t logical_sectors;
    uint16_t sectors_per_fat;
};

// FAT table (8bit)
struct FAT12_Table_Phys {
    uint8_t data[SECTOR_SIZE * FAT_TABLE_SECTOR_COUNT];
};

// FAT table (12bit)
struct FAT12_Table_Logic {
    uint16_t data[FAT_TABLE_LOGIC_SIZE];
};

// Sector buffer
struct Sector {
    uint8_t data[SECTOR_SIZE];
};

// File entry type
struct File_Entry {
    uint8_t name[8];
    uint8_t extension[3];
    uint8_t attribute;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t start_sector;
    uint32_t file_length;
};

// Sector Directory type
struct Sector_Dir {
    File_Entry entries[SECTOR_SIZE / sizeof(File_Entry)];
};

class File_Attribute {
    public:
        bool Read_only;
        bool Hidden;
        bool System_file;
        bool Volume_ID;
        bool Directory;
        bool Archive;
        uint8_t Reserved;

        File_Attribute();
};

// Date type
class Date {
    public:
        int year;
        int month;
        int day;

        Date();
};

// Time type
class Time {
    public:
        int hour;
        int minute;
        int second;

        Time();
};

// FAT12
class FAT12 {
    public:
        FAT12(uint8_t drive_id);

        Boot_Sector* boot_sector;
        FAT12_Table_Logic* table_logic;
        FAT12_Table_Phys* table_phys;
        uint8_t drive_id;

        static void Init_FAT12(std::unique_ptr<FAT12>& fat12, uint64_t number_of_sectors, uint16_t bytes_per_sector);
        static void Load_FAT12_Boot_Sector(std::unique_ptr<FAT12>& fat12, uint64_t number_of_sectors, uint16_t bytes_per_sector);
        static void Load_FAT12_Table(std::unique_ptr<FAT12>& fat12);
        static void Save_FAT12_Table(std::unique_ptr<FAT12>& fat12);
};

class Functions {
    public:
        static bool Create_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path, uint8_t attribute);
        static bool Remove_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path);
        static void Remove_Item_Data(std::unique_ptr<FAT12>& fat12, uint16_t start_sector);
        
        static bool Read_Item(std::unique_ptr<FAT12>& fat12, Sector* file, uint16_t start, uint64_t num, uint64_t count);
        static bool Write_Item(std::unique_ptr<FAT12>& fat12, Sector* file, uint16_t start, uint64_t num, uint64_t count);

        static bool Move_To_Dir(std::unique_ptr<FAT12>& fat12, std::string& working_dir, uint16_t& working_dir_sector, std::string new_path);

        // FOUNDED == TRUE && Return_path_item_count == 0 >> ROOT_DIR
        // FOUNDED == TRUE && Return_path_item_count == X >> ITEM
        // FOUNDED == FALSE && Return_path_item_count == ERROR_FIND >> PATH ERROR
        // FOUNDED == FALSE && Return_path_item_count == X >> PARENT ITEM
        static File_Entry* Get_Item(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path, std::string& return_path, int& return_path_item_count, bool& founded);
        static std::vector<File_Entry> Get_Items(std::unique_ptr<FAT12>& fat12, std::string working_dir, uint16_t working_dir_sector, std::string path);
        static std::vector<File_Entry> Get_Items(std::unique_ptr<FAT12>& fat12, uint16_t directory_sector);

        static int Compare_Filename(File_Entry item, std::string filename2);
        static int Compare_Filename(File_Entry item1, File_Entry item2);
        static uint64_t Get_Sectors_Count(std::unique_ptr<FAT12>& fat12, uint16_t start);
        static uint16_t Find_Sector(std::unique_ptr<FAT12>& fat12, uint64_t num, uint16_t actual_sector);
        static bool Next_Sector(std::unique_ptr<FAT12>& fat12, uint64_t* next_sector, uint64_t actual_sector);
        static File_Attribute* Read_Attribute(uint8_t attribute);

        static bool Read_Sectors(std::unique_ptr<FAT12>& fat12, uint64_t count, Sector* sector, uint64_t lba_index);
        static bool Write_Sectors(std::unique_ptr<FAT12>& fat12, uint64_t count, Sector* sector, uint64_t lba_index);
        static bool Process_Sectors(kiv_hal::NDisk_IO operation, uint8_t drive_id, uint64_t count, Sector* sector, uint64_t lba_index);
};

#endif FAT12_HEADER_H

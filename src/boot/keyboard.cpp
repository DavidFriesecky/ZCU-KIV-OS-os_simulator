#include "keyboard.h"
#include <Windows.h>

#include <iostream>
#include <queue>

HANDLE hConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
bool Std_In_Redirected = GetFileType(hConsoleInput) != FILE_TYPE_CHAR;
bool Std_In_Is_Open = true;

std::queue<decltype(kiv_hal::TRegisters::rax.x)> Keyboard_Buffer;
const size_t Max_Keyboard_Buffer_Size = 16;	//default BIOS size

bool Init_Keyboard() {
	//pokusime se vypnout echo na konzoli
	//mj. Win prestanou detekovat a "krast" napr. Ctrl+C

	if (Std_In_Redirected) return true;	//neni co prepinat s presmerovanym vstupem
		
	DWORD mode;
	bool echo_off = GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode);
	if (echo_off) echo_off = SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), mode & (~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT)));
	return echo_off;
}

bool Peek_Char() {
	if (!Keyboard_Buffer.empty()) //vlozil programator neco do bufferu klavesnice?
		return true;


	if (Std_In_Redirected) {		
		return Std_In_Is_Open;	//vracime globalni vlajku, protoze nemame jak otestovat znak v presmerovanem vstupu, aniz bychom ho precetli a tudi znicili
								//takze signalizujeme pritomnost EOT vraceneho v Read_Char se shozenym ZF, po kterem uz vzdy vratime false
	}

	INPUT_RECORD record;
	DWORD read;
	
	//zkontrolujeme prvni udalost ve frone, protoze jich tam muze byt vic a ruzneho typu
	if (PeekConsoleInputA(hConsoleInput, &record, 1, &read) && (read > 0)) {
		if (record.EventType == KEY_EVENT) 
			return true;		
	}

	return false;
}

bool Read_Char(decltype(kiv_hal::TRegisters::rax.x) &result_ch) {
	if (!Keyboard_Buffer.empty()) {		//nejprve prehrajeme, co mohl programator vlozit du bufferu klavesnice
		result_ch = Keyboard_Buffer.front();
		Keyboard_Buffer.pop();
		return true;
	}


	char ch;
	DWORD read;

	Std_In_Is_Open = ReadFile(hConsoleInput, &ch, 1, &read, NULL);

	if (Std_In_Is_Open)	//ReadConsoleA by neprecetlo presmerovany vstup ze souboru 
		result_ch = read > 0 ? ch : static_cast<std::remove_reference<decltype(result_ch)>::type>(kiv_hal::NControl_Codes::NUL);
	else 
		result_ch = static_cast<std::remove_reference<decltype(result_ch)>::type>(kiv_hal::NControl_Codes::EOT);	//chyba, patrne je zavren vstupni handle			
	
	return Std_In_Is_Open;
}

bool Write_Char(const decltype(kiv_hal::TRegisters::rax.x)& input_ch) {
	if (Keyboard_Buffer.size() >= Max_Keyboard_Buffer_Size)
		return false;

	if (!Std_In_Is_Open) 
		return false;

	Keyboard_Buffer.push(input_ch);
	return true;
}


void __stdcall Keyboard_Handler(kiv_hal::TRegisters &context) {
	switch (static_cast<kiv_hal::NKeyboard>(context.rax.h)) {
		case kiv_hal::NKeyboard::Peek_Char:	context.flags.non_zero = Peek_Char() ? 1 : 0;
											break;

		case kiv_hal::NKeyboard::Read_Char: context.flags.non_zero = Read_Char(context.rax.x) ? 1 : 0;
											break;


		case kiv_hal::NKeyboard::Write_Char: context.flags.non_zero = Write_Char(context.rax.x) ? 1 : 0;
											 break;

		default: context.flags.carry = 1;			
	}
}
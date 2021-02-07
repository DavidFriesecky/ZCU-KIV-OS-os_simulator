#pragma once

#include "..\api\api.h"
#include "rtl.h"
#include "command_executor.h"
#include "command_parser.h"

#define PATH_MAX_SIZE 4096

extern bool is_echo_enabled;

const char* intro = "Vitejte v KIV/OS.\n";
const char* new_line = "\n";
const char* prompt = "C:";
const char* beak = ">";

bool sig_shutdown = false;

extern "C" size_t __stdcall shell(const kiv_hal::TRegisters& regs);

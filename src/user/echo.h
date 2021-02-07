#pragma once

#include "..\api\api.h"
#include "rtl.h"
#include "command_header.h"

#define ECHO_ENABLE "@on"
#define ECHO_DISABLE "@off"

extern "C" size_t __stdcall echo(const kiv_hal::TRegisters& regs);
#pragma once

#include "..\api\api.h"
#include "rtl.h"
#include <ctime>

bool sig_terminate = false;
bool sig_eof = false;

extern "C" size_t __stdcall rgen(const kiv_hal::TRegisters& regs);
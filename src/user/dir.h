#pragma once

#include "..\api\api.h"
#include "rtl.h"
#include <string>
#include <bitset>
#include <algorithm>

#define PATH_MAX_SIZE 4096

extern "C" size_t __stdcall dir(const kiv_hal::TRegisters & regs);
#pragma once

#include "rtl.h"
#include "command_parser.h"

#include <map>

namespace Command_Executor {
	void Execute_Commands(std::vector<Command_Parser::Command>& commands, kiv_os::THandle handle_in, kiv_os::THandle handle_out);
}
#pragma once
#include <vector>
#include <string>
#include <sstream>

namespace Command_Parser {
	class Command {
		public:
			std::string base;
			std::string parameters;
			std::string filename;
			bool is_pipe = false;
			bool is_std_in = false;
			bool is_std_out = false;
	};

	std::vector<Command> Parse_Commands(std::string input);
}
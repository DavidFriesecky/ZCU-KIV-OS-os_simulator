#include "command_parser.h"

namespace Command_Parser {
	std::vector<Command> Parse_Commands(std::string input) {
		std::vector<Command> commands;
		std::stringstream stream(input);
		std::string token;

		bool stop_parse = false;

		while (!stop_parse) {
			if (!(stream >> token)) {
				break;
			}

			Command command;
			command.base = token;
			command.parameters = "";

			while (stream >> token) {
				if (token == "|") {
					command.is_pipe = true;
					break;
				}

				if (token == ">") {
					command.filename = "";
					while (stream >> token) {
						command.filename.append(token);
					}

					command.is_std_out = true;
					stop_parse = true;
					break;
				}

				if (token == "<") {
					command.filename = "";
					while (stream >> token) {
						command.filename.append(token);
					}

					command.is_std_in = true;
					stop_parse = true;
					break;
				}

				command.parameters.append(token);
				command.parameters.append(" ");
			}

			if (command.parameters.length() > 0 && command.parameters[command.parameters.length() - 1] == ' ') {
				command.parameters.pop_back();
			}
			commands.push_back(command);
		}

		return commands;
	}
}
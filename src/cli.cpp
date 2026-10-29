#include "car_terminal/cli.h"

#include <sstream>

#ifndef CAR_TERMINAL_VERSION
#define CAR_TERMINAL_VERSION "unknown"
#endif

namespace car_terminal {
namespace {

bool isOption(const std::string& argument) {
    return !argument.empty() && argument.front() == '-';
}

}  // namespace

CliOptions parseCommandLine(const std::vector<std::string>& arguments) {
    if (arguments.empty()) {
        return {};
    }

    if (arguments.front() == "--help" || arguments.front() == "-h") {
        if (arguments.size() != 1) {
            throw CliError("--help does not accept additional arguments");
        }
        return {CliAction::ShowHelp, {}};
    }

    if (arguments.front() == "--version") {
        if (arguments.size() != 1) {
            throw CliError("--version does not accept additional arguments");
        }
        return {CliAction::ShowVersion, {}};
    }

    if (arguments.front() == "--config") {
        if (arguments.size() == 1) {
            throw CliError("--config requires a file path");
        }
        if (arguments.size() != 2) {
            throw CliError("--config accepts exactly one file path");
        }
        if (arguments[1].empty()) {
            throw CliError("--config path must not be empty");
        }
        return {CliAction::Run, arguments[1]};
    }

    if (isOption(arguments.front())) {
        throw CliError("Unknown option: " + arguments.front());
    }
    if (arguments.size() != 1) {
        throw CliError("Only one positional configuration file is allowed");
    }
    return {CliAction::Run, arguments.front()};
}

std::string commandLineUsage(const std::string& programName) {
    std::ostringstream output;
    output << "Usage:\n"
           << "  " << programName << " [configuration-file]\n"
           << "  " << programName << " --config <path>\n"
           << "  " << programName << " --help\n"
           << "  " << programName << " --version\n";
    return output.str();
}

std::string applicationVersion() {
    return CAR_TERMINAL_VERSION;
}

}  // namespace car_terminal

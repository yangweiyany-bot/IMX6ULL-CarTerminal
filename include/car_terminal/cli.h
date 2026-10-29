#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace car_terminal {

enum class CliAction {
    Run,
    ShowHelp,
    ShowVersion,
};

struct CliOptions {
    CliAction action{CliAction::Run};
    std::filesystem::path configPath{"config/default.conf"};
};

class CliError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

CliOptions parseCommandLine(const std::vector<std::string>& arguments);
std::string commandLineUsage(const std::string& programName);
std::string applicationVersion();

}  // namespace car_terminal

#include "car_terminal/cli.h"
#include "test_support.h"

#include <string>
#include <vector>

void runCliTests(test_support::TestSuite& suite) {
    using namespace test_support;

    suite.run("CLI default configuration", [] {
        const auto options = car_terminal::parseCommandLine({});
        require(options.action == car_terminal::CliAction::Run,
                "default action should run");
        require(options.configPath == "config/default.conf",
                "default configuration path");
    });

    suite.run("CLI explicit configuration", [] {
        const auto options = car_terminal::parseCommandLine(
            {"--config", "/tmp/car.conf"});
        require(options.action == car_terminal::CliAction::Run,
                "--config action should run");
        require(options.configPath == "/tmp/car.conf",
                "--config path should be preserved");
    });

    suite.run("CLI positional configuration compatibility", [] {
        const auto options = car_terminal::parseCommandLine({"legacy.conf"});
        require(options.action == car_terminal::CliAction::Run,
                "positional action should run");
        require(options.configPath == "legacy.conf",
                "positional path should be preserved");
    });

    suite.run("CLI help and version actions", [] {
        const auto help = car_terminal::parseCommandLine({"--help"});
        const auto shortHelp = car_terminal::parseCommandLine({"-h"});
        const auto version = car_terminal::parseCommandLine({"--version"});
        require(help.action == car_terminal::CliAction::ShowHelp,
                "--help action");
        require(shortHelp.action == car_terminal::CliAction::ShowHelp,
                "-h action");
        require(version.action == car_terminal::CliAction::ShowVersion,
                "--version action");
        require(!car_terminal::applicationVersion().empty(),
                "application version should not be empty");
        require(car_terminal::commandLineUsage("car_terminal").find(
                    "--config <path>") != std::string::npos,
                "usage should document --config");
    });

    suite.run("CLI rejects invalid arguments", [] {
        requireThrows<car_terminal::CliError>(
            [] { static_cast<void>(car_terminal::parseCommandLine({"--bad"})); },
            "Unknown option");
        requireThrows<car_terminal::CliError>(
            [] {
                static_cast<void>(
                    car_terminal::parseCommandLine({"--config"}));
            },
            "requires a file path");
        requireThrows<car_terminal::CliError>(
            [] {
                static_cast<void>(car_terminal::parseCommandLine(
                    {"--config", "one.conf", "two.conf"}));
            },
            "exactly one");
        requireThrows<car_terminal::CliError>(
            [] {
                static_cast<void>(car_terminal::parseCommandLine(
                    {"one.conf", "two.conf"}));
            },
            "Only one positional");
    });
}

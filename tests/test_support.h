#pragma once

#include <atomic>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace test_support {

class TestFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

inline void require(bool condition, const std::string& message) {
    if (!condition) {
        throw TestFailure(message);
    }
}

template <typename Actual, typename Expected>
void requireEqual(const Actual& actual,
                  const Expected& expected,
                  const std::string& message) {
    if (!(actual == expected)) {
        std::ostringstream detail;
        detail << message << " (actual=" << actual
               << ", expected=" << expected << ')';
        throw TestFailure(detail.str());
    }
}

inline void requireNear(double actual,
                        double expected,
                        double tolerance,
                        const std::string& message) {
    if (std::fabs(actual - expected) > tolerance) {
        std::ostringstream detail;
        detail << message << " (actual=" << actual
               << ", expected=" << expected << ')';
        throw TestFailure(detail.str());
    }
}

template <typename Exception, typename Function>
void requireThrows(Function&& function,
                   const std::string& expectedMessageFragment) {
    try {
        function();
    } catch (const Exception& error) {
        if (!expectedMessageFragment.empty() &&
            std::string(error.what()).find(expectedMessageFragment) ==
                std::string::npos) {
            throw TestFailure("Exception message did not contain '" +
                              expectedMessageFragment + "': " + error.what());
        }
        return;
    } catch (const std::exception& error) {
        throw TestFailure("Unexpected exception type: " +
                          std::string(error.what()));
    }
    throw TestFailure("Expected exception was not thrown");
}

class TempDirectory {
public:
    TempDirectory() {
        static std::atomic<unsigned long> counter{0};
        const auto identifier = counter.fetch_add(1);
        path_ = std::filesystem::temp_directory_path() /
                ("imx6ull_car_terminal_test_" +
                 std::to_string(identifier));
        std::error_code error;
        std::filesystem::remove_all(path_, error);
        error.clear();
        std::filesystem::create_directories(path_, error);
        if (error) {
            throw std::runtime_error("Cannot create test directory: " +
                                     error.message());
        }
    }

    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    std::filesystem::path path_;
};

inline void writeText(const std::filesystem::path& path,
                      const std::string& content) {
    std::ofstream output(path);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create test file: " + path.string());
    }
    output << content;
    if (!output) {
        throw std::runtime_error("Cannot write test file: " + path.string());
    }
}

inline std::string readText(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot read test file: " + path.string());
    }
    std::ostringstream content;
    content << input.rdbuf();
    return content.str();
}

class TestSuite {
public:
    void run(const std::string& name, const std::function<void()>& test) {
        try {
            test();
            ++passed_;
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed_;
            std::cerr << "[FAIL] " << name << ": " << error.what() << '\n';
        } catch (...) {
            ++failed_;
            std::cerr << "[FAIL] " << name
                      << ": unknown non-standard exception\n";
        }
    }

    int finish() const {
        std::cout << "Tests passed: " << passed_
                  << ", failed: " << failed_ << '\n';
        return failed_ == 0 ? 0 : 1;
    }

private:
    int passed_{};
    int failed_{};
};

}  // namespace test_support

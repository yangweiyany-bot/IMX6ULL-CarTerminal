cmake_minimum_required(VERSION 3.16)

project(IMX6ULL_CarTerminal VERSION 2.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
include(GNUInstallDirs)
include(CheckIncludeFileCXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "SocketCAN support requires a Linux target")
endif()
check_include_file_cxx("linux/can.h" CAR_TERMINAL_HAVE_LINUX_CAN_H)
check_include_file_cxx("linux/can/raw.h" CAR_TERMINAL_HAVE_LINUX_CAN_RAW_H)
if(NOT CAR_TERMINAL_HAVE_LINUX_CAN_H OR
   NOT CAR_TERMINAL_HAVE_LINUX_CAN_RAW_H)
    message(FATAL_ERROR
        "SocketCAN support requires linux/can.h and linux/can/raw.h")
endif()

set(CAR_TERMINAL_INSTALL_NAME "imx6ull-car-terminal")
set(CAR_TERMINAL_INSTALL_CONFIG_DIR
    "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CAR_TERMINAL_INSTALL_NAME}")
set(CAR_TERMINAL_INSTALL_DOC_DIR
    "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CAR_TERMINAL_INSTALL_NAME}")
set(CAR_TERMINAL_INSTALL_SYSTEMD_DIR
    "lib/systemd/system")
set(CAR_TERMINAL_INSTALL_SUPPORT_DIR
    "${CMAKE_INSTALL_DATAROOTDIR}/${CAR_TERMINAL_INSTALL_NAME}")

set(CAR_TERMINAL_INSTALL_CONFIG_ABSOLUTE_DIR
    "${CMAKE_INSTALL_FULL_SYSCONFDIR}/${CAR_TERMINAL_INSTALL_NAME}")
set(CAR_TERMINAL_INSTALL_LOG_ABSOLUTE_FILE
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/log/${CAR_TERMINAL_INSTALL_NAME}/car_terminal.log")
set(CAR_TERMINAL_INSTALL_CSV_ABSOLUTE_FILE
    "${CMAKE_INSTALL_FULL_LOCALSTATEDIR}/lib/${CAR_TERMINAL_INSTALL_NAME}/vehicle_data.csv")
file(RELATIVE_PATH CAR_TERMINAL_INSTALL_LOG_PATH
    "${CAR_TERMINAL_INSTALL_CONFIG_ABSOLUTE_DIR}"
    "${CAR_TERMINAL_INSTALL_LOG_ABSOLUTE_FILE}")
file(RELATIVE_PATH CAR_TERMINAL_INSTALL_CSV_PATH
    "${CAR_TERMINAL_INSTALL_CONFIG_ABSOLUTE_DIR}"
    "${CAR_TERMINAL_INSTALL_CSV_ABSOLUTE_FILE}")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(
    config/default-install.conf.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/default.conf"
    @ONLY
)
set(CAR_TERMINAL_SYSTEMD_LOG_FILE
    "/var/log/${CAR_TERMINAL_INSTALL_NAME}/car_terminal.log")
set(CAR_TERMINAL_SYSTEMD_CSV_FILE
    "/var/lib/${CAR_TERMINAL_INSTALL_NAME}/vehicle_data.csv")
configure_file(
    config/systemd-simulator.conf.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/systemd.conf"
    @ONLY
)
configure_file(
    packaging/systemd/imx6ull-car-terminal.service.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/imx6ull-car-terminal.service"
    @ONLY
)

function(enable_strict_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endfunction()

add_library(car_terminal_core
    src/can_parser.cpp
    src/cli.cpp
    src/configuration.cpp
    src/csv_recorder.cpp
    src/file_rotation.cpp
    src/logger.cpp
    src/monitoring_service.cpp
    src/sample_processor.cpp
    src/simulator.cpp
    src/signal_handler.cpp
    src/socket_can_receiver.cpp
    src/socket_can_source.cpp
    src/status_checker.cpp
)

target_include_directories(car_terminal_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
enable_strict_warnings(car_terminal_core)
target_link_libraries(car_terminal_core PUBLIC Threads::Threads)
target_compile_definitions(car_terminal_core
    PUBLIC CAR_TERMINAL_VERSION="${PROJECT_VERSION}"
)

add_executable(car_terminal main.cpp)
target_link_libraries(car_terminal PRIVATE car_terminal_core)
enable_strict_warnings(car_terminal)

install(TARGETS car_terminal
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT Runtime
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/default.conf"
    DESTINATION "${CAR_TERMINAL_INSTALL_CONFIG_DIR}"
    COMPONENT Runtime
)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/systemd.conf"
    DESTINATION "${CAR_TERMINAL_INSTALL_CONFIG_DIR}"
    COMPONENT Runtime
)
install(FILES
    config/continuous-simulator-example.conf
    config/socketcan-synthetic-example.conf
    DESTINATION "${CAR_TERMINAL_INSTALL_CONFIG_DIR}"
    COMPONENT Runtime
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/generated/imx6ull-car-terminal.service"
    DESTINATION "${CAR_TERMINAL_INSTALL_SYSTEMD_DIR}"
    COMPONENT Runtime
)
install(FILES packaging/systemd/socketcan-capability.conf
    DESTINATION "${CAR_TERMINAL_INSTALL_SUPPORT_DIR}/systemd"
    COMPONENT Runtime
)
install(PROGRAMS
    scripts/install-systemd.sh
    scripts/uninstall-systemd.sh
    DESTINATION "${CAR_TERMINAL_INSTALL_SUPPORT_DIR}/scripts"
    COMPONENT Runtime
)
install(FILES
    README.md
    docs/IMX6ULL_PORTING_GUIDE.md
    docs/IMX6ULL_CROSS_COMPILE.md
    docs/SYSTEMD_DEPLOYMENT.md
    docs/SOCKETCAN_VIRTUAL_PLAN.md
    DESTINATION "${CAR_TERMINAL_INSTALL_DOC_DIR}"
    COMPONENT Documentation
)

include(CTest)
if(BUILD_TESTING)
    add_executable(car_terminal_tests
        tests/test_blocking_queue.cpp
        tests/test_can_parser.cpp
        tests/test_cli.cpp
        tests/test_configuration.cpp
        tests/test_io.cpp
        tests/test_main.cpp
        tests/test_signal.cpp
        tests/test_socket_can.cpp
        tests/test_status.cpp
        tests/test_threading.cpp
    )
    target_link_libraries(car_terminal_tests PRIVATE car_terminal_core)
    target_include_directories(car_terminal_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )
    enable_strict_warnings(car_terminal_tests)
    add_test(NAME car_terminal_tests COMMAND car_terminal_tests)
    set_tests_properties(car_terminal_tests PROPERTIES TIMEOUT 30)

    find_program(BASH_EXECUTABLE bash REQUIRED)
    add_test(
        NAME car_terminal_signal_shutdown
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_signal_shutdown.sh"
                "$<TARGET_FILE:car_terminal>"
    )
    set_tests_properties(car_terminal_signal_shutdown PROPERTIES TIMEOUT 30)

    add_test(
        NAME car_terminal_systemd_scripts
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_systemd_scripts.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/install-systemd.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/uninstall-systemd.sh"
                "${CMAKE_CURRENT_BINARY_DIR}/generated/imx6ull-car-terminal.service"
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/systemd/socketcan-capability.conf"
    )
    set_tests_properties(car_terminal_systemd_scripts PROPERTIES TIMEOUT 30)

    add_test(
        NAME car_terminal_systemd_unit
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_systemd_unit.sh"
                "${CMAKE_CURRENT_BINARY_DIR}/generated/imx6ull-car-terminal.service"
    )
    set_tests_properties(car_terminal_systemd_unit PROPERTIES TIMEOUT 30)

    add_test(
        NAME car_terminal_cross_build_scripts
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cross_build_scripts.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/build-imx6ull.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/scripts/check-arm-elf.sh"
                "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/imx6ull-linux-gnu.cmake"
                "$<TARGET_FILE:car_terminal>"
    )
    set_tests_properties(car_terminal_cross_build_scripts PROPERTIES
        TIMEOUT 30
    )

    add_executable(socketcan_test_sender tests/socketcan_test_sender.cpp)
    enable_strict_warnings(socketcan_test_sender)
    add_test(
        NAME car_terminal_vcan_integration
        COMMAND "${BASH_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_vcan_integration.sh"
                "$<TARGET_FILE:car_terminal>"
                "$<TARGET_FILE:socketcan_test_sender>"
    )
    set_tests_properties(car_terminal_vcan_integration PROPERTIES
        TIMEOUT 30
        SKIP_RETURN_CODE 77
        LABELS "integration;socketcan"
    )
endif()

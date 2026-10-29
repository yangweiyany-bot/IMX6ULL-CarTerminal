cmake_minimum_required(VERSION 3.16)

project(IMX6ULL_CarTerminal VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
include(GNUInstallDirs)

set(CAR_TERMINAL_INSTALL_NAME "imx6ull-car-terminal")
set(CAR_TERMINAL_INSTALL_CONFIG_DIR
    "${CMAKE_INSTALL_SYSCONFDIR}/${CAR_TERMINAL_INSTALL_NAME}")
set(CAR_TERMINAL_INSTALL_DOC_DIR
    "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CAR_TERMINAL_INSTALL_NAME}")

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

function(enable_strict_warnings target_name)
    target_compile_options(${target_name} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endfunction()

add_library(car_terminal_core
    src/cli.cpp
    src/configuration.cpp
    src/csv_recorder.cpp
    src/logger.cpp
    src/monitoring_service.cpp
    src/sample_processor.cpp
    src/simulator.cpp
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
install(FILES
    README.md
    docs/IMX6ULL_PORTING_GUIDE.md
    DESTINATION "${CAR_TERMINAL_INSTALL_DOC_DIR}"
    COMPONENT Documentation
)

include(CTest)
if(BUILD_TESTING)
    add_executable(car_terminal_tests
        tests/test_blocking_queue.cpp
        tests/test_cli.cpp
        tests/test_configuration.cpp
        tests/test_io.cpp
        tests/test_main.cpp
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
endif()

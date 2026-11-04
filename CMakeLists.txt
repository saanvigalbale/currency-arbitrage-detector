cmake_minimum_required(VERSION 3.20)

project(CurrencyArbitrageDetector
    VERSION 1.0.0
    DESCRIPTION "High-performance Currency Arbitrage Detection System in C++"
    LANGUAGES CXX
)

# Enforce C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Core library for graph, arbitrage, loader, and reporter logic
add_library(arbitrage_core STATIC
    src/CurrencyGraph.cpp
    src/ArbitrageDetector.cpp
    src/RateLoader.cpp
    src/ArbitrageReporter.cpp
)

target_include_directories(arbitrage_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(arbitrage_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(arbitrage_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Main application executable
add_executable(currency_arbitrage
    src/main.cpp
)

target_link_libraries(currency_arbitrage PRIVATE arbitrage_core)

if(MSVC)
    target_compile_options(currency_arbitrage PRIVATE /W4 /permissive-)
else()
    target_compile_options(currency_arbitrage PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Unit and Integration Tests
enable_testing()

# 1. Graph structure and rate validation tests
add_executable(test_graph
    tests/test_graph.cpp
)
target_link_libraries(test_graph PRIVATE arbitrage_core)

if(MSVC)
    target_compile_options(test_graph PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_graph PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME TestCurrencyGraph COMMAND test_graph)

# 2. Bellman-Ford arbitrage detection tests
add_executable(test_arbitrage
    tests/test_arbitrage.cpp
)
target_link_libraries(test_arbitrage PRIVATE arbitrage_core)

if(MSVC)
    target_compile_options(test_arbitrage PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_arbitrage PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME TestArbitrageDetector COMMAND test_arbitrage)

# 3. CSV RateLoader tests
add_executable(test_rate_loader
    tests/test_rate_loader.cpp
)
target_link_libraries(test_rate_loader PRIVATE arbitrage_core)

if(MSVC)
    target_compile_options(test_rate_loader PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_rate_loader PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME TestRateLoader COMMAND test_rate_loader)

# 4. End-to-end integration and reporter tests
add_executable(test_end_to_end
    tests/test_end_to_end.cpp
)
target_link_libraries(test_end_to_end PRIVATE arbitrage_core)

if(MSVC)
    target_compile_options(test_end_to_end PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_end_to_end PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME TestEndToEnd COMMAND test_end_to_end)

# 5. Automated actual CLI executable integration tests
add_executable(test_cli
    tests/test_cli.cpp
)
add_dependencies(test_cli currency_arbitrage)

if(MSVC)
    target_compile_options(test_cli PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME TestCliExecutable COMMAND test_cli)

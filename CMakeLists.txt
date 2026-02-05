cmake_minimum_required(VERSION 3.16)
project(minecraft_server_manager
    LANGUAGES C CXX
)

find_package(CLI11 REQUIRED)
find_package(nlohmann_json REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(nlohmann_json REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

add_executable(test_1
    src/test.cpp
    src/http.cpp
    src/systemd.cpp
)

target_include_directories(test_1
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${SYSTEMD_INCLUDE_DIRS}
)

target_link_libraries(test_1
    Boost::system
    Boost::thread
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)

add_executable(minecraft_server_manager
    src/main.cpp
    src/http.cpp
    src/systemd.cpp
)

target_include_directories(minecraft_server_manager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${SYSTEMD_INCLUDE_DIRS}
)

target_link_libraries(minecraft_server_manager
    Boost::system
    Boost::thread
    nlohmann_json::nlohmann_json
    ${SYSTEMD_LIBRARIES}
)

install(TARGETS minecraft_server_manager
    RUNTIME DESTINATION bin
)

# install(DIRECTORY include/
#     DESTINATION include
# )

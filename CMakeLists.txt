cmake_minimum_required(VERSION 3.4)
project(sysfs_fw_loader)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONC REQUIRED json-c)

add_executable(sysfs_fw_loader main.cpp)
target_link_libraries(sysfs_fw_loader ${JSONC_LIBRARIES})

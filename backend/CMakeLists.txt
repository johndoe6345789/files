cmake_minimum_required(VERSION 3.16)
project(files_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)

# Everything except main.cc, so the test binary links the same code the server runs. OBJECT (not STATIC) so the
# controller's static registration is never dropped by the linker.
add_library(files_lib OBJECT src/Util.cc src/Storage.cc src/FilesController.cc)
target_include_directories(files_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(files_lib PUBLIC Drogon::Drogon)

add_executable(files_backend src/main.cc)
target_link_libraries(files_backend PRIVATE files_lib)

add_executable(util_test tests/util_test.cc)
target_link_libraries(util_test PRIVATE files_lib)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/config.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

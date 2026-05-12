cmake_minimum_required(VERSION 4.3)

project(ticketeer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(quill REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(
  ticketeer
  src/ticketeer.cc
  src/ticketeer/core/conf.cpp
  src/ticketeer/core/logging.cpp
  src/ticketeer/handling/auth/middlewares.cpp
  src/ticketeer/handling/auth/pbkdf2.cpp
  src/ticketeer/handling/auth/routes.cpp
  src/ticketeer/handling/dashboard/routes.cpp
  src/ticketeer/handling/role/requester/middlewares.cpp
  src/ticketeer/handling/role/requester/routes.cpp
  src/ticketeer/handling/role/supervisor/middlewares.cpp
  src/ticketeer/handling/role/supervisor/routes.cpp)

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/requester/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/supervisor/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/auth/template
  ${CMAKE_CURRENT_BINARY_DIR})

add_executable(
  ticketeer-create_user
  cmd/ticketeer-create_user.cc)

target_include_directories(
  ticketeer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  ticketeer SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(ticketeer PRIVATE Drogon::Drogon
                                        quill::quill ZLIB::ZLIB sqlite3)

target_include_directories(
  ticketeer-create_user PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  ticketeer-create_user SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(ticketeer-create_user PRIVATE sqlite3)

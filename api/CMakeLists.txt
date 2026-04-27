cmake_minimum_required(VERSION 4.3)

project(ticketeer-api LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(quill REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(
  ticketeer-api
  src/ticketeer-api.cc
  src/ticketeer/core/conf.cpp
  src/ticketeer/handling/auth/middlewares.cpp
  src/ticketeer/handling/auth/routes.cpp
  src/ticketeer/handling/dashboard/routes.cpp
  src/ticketeer/handling/role/requester/middlewares.cpp
  src/ticketeer/handling/role/requester/routes.cpp
  src/ticketeer/handling/role/supervisor/middlewares.cpp
  src/ticketeer/handling/role/supervisor/routes.cpp)

target_include_directories(
  ticketeer-api PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
                        ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(
  ticketeer-api PRIVATE Drogon::Drogon PostgreSQL::PostgreSQL quill::quill
                        ZLIB::ZLIB)

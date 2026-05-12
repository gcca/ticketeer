cmake_minimum_required(VERSION 4.3)

project(ticketeer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
  set(CMAKE_CXX_COMPILER_LAUNCHER
    ${CMAKE_COMMAND} -E env
    CCACHE_SLOPPINESS=pch_defines,time_macros
    "CCACHE_BASEDIR=${CMAKE_SOURCE_DIR}"
    "${CCACHE_PROGRAM}")
endif()

option(TICKETEER_TEST "Build tests" OFF)
option(TICKETEER_PULSE "Compile the private pulse scheduler" OFF)
option(TICKETEER_SENTRY "Compile Sentry crash reporting" OFF)
set(TICKETEER_PULSE_DIR "" CACHE PATH "Directory containing pulse")

find_package(Drogon CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
if(TICKETEER_SENTRY)
  find_package(sentry CONFIG REQUIRED)
endif()

add_executable(
  ticketeer
  src/ticketeer.cc
  src/ticketeer/core/conf.cpp
  src/ticketeer/core/db.cpp
  src/ticketeer/core/logging.cpp
  src/ticketeer/handling/common.cpp
  src/ticketeer/handling/utils.cpp
  src/ticketeer/handling/auth/middlewares.cpp
  src/ticketeer/handling/auth/pbkdf2.cpp
  src/ticketeer/handling/auth/routes.cpp
  src/ticketeer/handling/base/routes.cpp
  src/ticketeer/handling/dashboard/routes.cpp
  src/ticketeer/handling/role/requester/middlewares.cpp
  src/ticketeer/handling/role/requester/routes/common.cpp
  src/ticketeer/handling/role/requester/routes/home_get.cpp
  src/ticketeer/handling/role/requester/routes/ticket_activity_attachment_create_post.cpp
  src/ticketeer/handling/role/requester/routes/ticket_activity_attachment_download_get.cpp
  src/ticketeer/handling/role/requester/routes/ticket_activity_create_message_post.cpp
  src/ticketeer/handling/role/requester/routes/ticket_activity_list_get.cpp
  src/ticketeer/handling/role/requester/routes/ticket_create_get.cpp
  src/ticketeer/handling/role/requester/routes/ticket_create_post.cpp
  src/ticketeer/handling/role/requester/routes/ticket_details_get.cpp
  src/ticketeer/handling/role/requester/routes/ticket_list_get.cpp
  src/ticketeer/handling/role/technician/middlewares.cpp
  src/ticketeer/handling/role/technician/routes/common.cpp
  src/ticketeer/handling/role/technician/routes/home_get.cpp
  src/ticketeer/handling/role/technician/routes/ticket_activity_attachment_create_post.cpp
  src/ticketeer/handling/role/technician/routes/ticket_activity_attachment_download_get.cpp
  src/ticketeer/handling/role/technician/routes/ticket_activity_create_message_post.cpp
  src/ticketeer/handling/role/technician/routes/ticket_activity_list_get.cpp
  src/ticketeer/handling/role/technician/routes/ticket_create_get.cpp
  src/ticketeer/handling/role/technician/routes/ticket_create_post.cpp
  src/ticketeer/handling/role/technician/routes/ticket_details_get.cpp
  src/ticketeer/handling/role/technician/routes/ticket_list_get.cpp
  src/ticketeer/handling/role/supervisor/middlewares.cpp
  src/ticketeer/handling/role/supervisor/routes/common.cpp
  src/ticketeer/handling/role/supervisor/routes/config_settings_get.cpp
  src/ticketeer/handling/role/supervisor/routes/config_settings_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_priority_create_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_priority_delete_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_priority_list_get.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_priority_update_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_status_create_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_status_delete_post.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_status_list_get.cpp
  src/ticketeer/handling/role/supervisor/routes/config_ticket_status_update_post.cpp
  src/ticketeer/handling/role/supervisor/routes/home_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_activity_attachment_create_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_activity_attachment_download_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_activity_create_message_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_activity_list_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_assigned_to_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_create_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_create_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_details_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_due_date_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_list_get.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_priority_post.cpp
  src/ticketeer/handling/role/supervisor/routes/ticket_status_post.cpp
  src/ticketeer/notifications/clickup.cpp)

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/requester/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/technician/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/supervisor/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/auth/template
  ${CMAKE_CURRENT_BINARY_DIR})

target_include_directories(
  ticketeer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  ticketeer SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(ticketeer PRIVATE Drogon::Drogon ZLIB::ZLIB sqlite3)

if(TICKETEER_SENTRY)
  target_sources(ticketeer PRIVATE src/ticketeer/core/sentry.cpp)
  target_link_libraries(ticketeer PRIVATE sentry::sentry)
  target_compile_definitions(ticketeer PRIVATE TICKETEER_SENTRY)
endif()

if(TICKETEER_PULSE)
  if(NOT TICKETEER_PULSE_DIR)
    message(FATAL_ERROR "TICKETEER_PULSE=ON requires TICKETEER_PULSE_DIR")
  endif()
  target_sources(ticketeer PRIVATE ${TICKETEER_PULSE_DIR}/pulse.cpp)
  target_include_directories(ticketeer PRIVATE ${TICKETEER_PULSE_DIR})
  target_include_directories(ticketeer SYSTEM PRIVATE
                             ${TICKETEER_PULSE_DIR}/../../3rdparty)
  target_compile_definitions(ticketeer PRIVATE TICKETEER_PULSE)
endif()

target_precompile_headers(
  ticketeer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer_pch.h)

if(TICKETEER_TEST)
  find_package(GTest REQUIRED)

  enable_testing()

  add_executable(
    ticketeer-test
    src/ticketeer/core/conf-test.cc
    src/ticketeer/core/logging.cpp
    src/ticketeer/core/logging-test.cc
    src/ticketeer/core/options-test.cc
    src/ticketeer/handling/auth/pbkdf2.cpp
    src/ticketeer/handling/auth/utils-test.cc)

  target_include_directories(
    ticketeer-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_include_directories(
    ticketeer-test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
  target_link_libraries(ticketeer-test PRIVATE Drogon::Drogon
    GTest::gtest_main
    ZLIB::ZLIB sqlite3)

  include(GoogleTest)
  gtest_discover_tests(ticketeer-test)
endif()

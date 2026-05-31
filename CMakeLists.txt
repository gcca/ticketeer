cmake_minimum_required(VERSION 4.3)

project(ticketeer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(quill REQUIRED)
find_package(ZLIB REQUIRED)

option(TICKETEER_TEST "Build tests" OFF)

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
  src/ticketeer/handling/role/supervisor/routes/ticket_status_post.cpp)

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/requester/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/role/supervisor/template
  ${CMAKE_CURRENT_BINARY_DIR})

drogon_create_views(ticketeer
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ticketeer/handling/auth/template
  ${CMAKE_CURRENT_BINARY_DIR})

function(ticketeer_add_command name)
  add_executable(${name} cmd/${name}.cc)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_include_directories(${name} SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
  target_link_libraries(${name} PRIVATE sqlite3)
endfunction()

target_include_directories(
  ticketeer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(
  ticketeer SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
target_link_libraries(ticketeer PRIVATE Drogon::Drogon
  quill::quill ZLIB::ZLIB sqlite3)

ticketeer_add_command(ticketeer-user_create)
ticketeer_add_command(ticketeer-user_list)
ticketeer_add_command(ticketeer-user_password)

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
    quill::quill ZLIB::ZLIB sqlite3)

  include(GoogleTest)
  gtest_discover_tests(ticketeer-test)
endif()

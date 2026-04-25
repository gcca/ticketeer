cmake_minimum_required(VERSION 4.3.1)

project(ticketeer-api VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(Asio REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(quill REQUIRED)

add_executable(ticketeer-api
  src/ticketeer-api.cc
  src/ticketeer/core/options.cpp
  src/ticketeer/core/settings.cpp
  src/ticketeer/core/log.cpp
  src/ticketeer/db/result.cpp
  src/ticketeer/db/pg.cpp
  src/ticketeer/apps/helpdesk/roles/supervisor/handlers/assign-tickets.cpp
  src/ticketeer/apps/auth/internal.cpp
  src/ticketeer/apps/helpdesk/roles/requester/internal.cpp
  src/ticketeer/apps/helpdesk/roles/administrator/internal.cpp
  src/ticketeer/apps/helpdesk/ticket-statuses/internal.cpp
  src/ticketeer/apps/helpdesk/request-types/internal.cpp
  src/ticketeer/apps/helpdesk/roles/technician/internal.cpp
  src/ticketeer/apps/helpdesk/tickets/internal.cpp
  src/ticketeer/apps/helpdesk/profiles/internal.cpp
  src/ticketeer/apps/helpdesk/priorities/internal.cpp
  src/ticketeer/apps/helpdesk/departments/internal.cpp
  src/ticketeer/apps/helpdesk/roles/internal.cpp
  src/ticketeer/apps/helpdesk/roles/supervisor/internal.cpp
)
target_include_directories(ticketeer-api SYSTEM PRIVATE 3rdparty)
target_include_directories(ticketeer-api PRIVATE src)
target_link_libraries(ticketeer-api PRIVATE asio::asio PostgreSQL::PostgreSQL quill::quill)


option(TICKETEER_API_COMMANDS "Build management commands" OFF)

if(TICKETEER_API_COMMANDS)
  set(TICKETEER_API_COMMANDS_LIST
    create-user
  )

  foreach(cmd ${TICKETEER_API_COMMANDS_LIST})
    add_executable(ticketeer_cmd_${cmd} src/commands/${cmd}.cc)
    target_include_directories(ticketeer_cmd_${cmd} SYSTEM PRIVATE 3rdparty)
    target_include_directories(ticketeer_cmd_${cmd} PRIVATE src)
    target_link_libraries(ticketeer_cmd_${cmd} PRIVATE PostgreSQL::PostgreSQL)
  endforeach()
endif()

option(TICKETEER_API_TESTS "Build and enable tests" OFF)

if(TICKETEER_API_TESTS)
  find_package(GTest REQUIRED)
  enable_testing()

  add_executable(ticketeer-api-test
    src/ticketeer/apps/auth/auth-test.cc
    src/ticketeer/core/settings.cpp
  )
  target_include_directories(ticketeer-api-test SYSTEM PRIVATE 3rdparty)
  target_include_directories(ticketeer-api-test PRIVATE src)
  target_link_libraries(ticketeer-api-test PRIVATE
    asio::asio
    GTest::gtest_main
    GTest::gmock
  )
  add_test(NAME ticketeer-api-test COMMAND ticketeer-api-test)
endif()

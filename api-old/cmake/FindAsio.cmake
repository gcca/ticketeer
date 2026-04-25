find_path(Asio_INCLUDE_DIR
  NAMES asio.hpp
  HINTS ENV ASIO_ROOT
  PATH_SUFFIXES include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Asio REQUIRED_VARS Asio_INCLUDE_DIR)

if(Asio_FOUND AND NOT TARGET asio::asio)
  add_library(asio::asio INTERFACE IMPORTED)
  set_target_properties(asio::asio PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${Asio_INCLUDE_DIR}"
  )
endif()

find_path(TGUI_INCLUDE_DIR TGUI/TGUI.hpp
  PATHS
    /usr/include
    /usr/local/include
)

find_library(TGUI_LIBRARY
  NAMES tgui
  PATHS
    /usr/lib/x86_64-linux-gnu
    /usr/local/lib/x86_64-linux-gnu
    /usr/lib
    /usr/local/lib
)

# Set version (TGUI 1.10.0 is installed)
set(TGUI_VERSION "1.10.0")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TGUI
  REQUIRED_VARS TGUI_LIBRARY TGUI_INCLUDE_DIR
  VERSION_VAR TGUI_VERSION
)

if(TGUI_FOUND)
  add_library(TGUI::TGUI UNKNOWN IMPORTED)
  set_target_properties(TGUI::TGUI PROPERTIES
    IMPORTED_LOCATION "${TGUI_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${TGUI_INCLUDE_DIR}"
    INTERFACE_LINK_LIBRARIES "${TGUI_LIBRARY}"
  )
endif()

mark_as_advanced(TGUI_INCLUDE_DIR TGUI_LIBRARY)
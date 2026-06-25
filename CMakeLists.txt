cmake_minimum_required(VERSION 3.16...3.29)
project(OpenSkyscraper)
cmake_policy(SET CMP0012 NEW)  # if() recognizes TRUE/FALSE/ON/OFF
cmake_policy(SET CMP0023 NEW)  # use keyword target_link_libraries
# Executable name.
set(EXECUTABLE_NAME OpenSkyscraper)

add_compile_definitions(
  $<$<CONFIG:Debug>:BUILD_DEBUG>
  $<$<NOT:$<CONFIG:Debug>>:BUILD_RELEASE>
)

# Tests
enable_testing()

# Gather the source files
file(GLOB_RECURSE sources CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/source/*.cpp
  ${PROJECT_SOURCE_DIR}/src/*.cpp
)

# Include AStar-Search headers
include_directories(thirdparty/astar-search/)

###  PLATFORM-SPECIFIC STUFF
###  =======================

# Mac OS X
if (APPLE)
	add_definitions("-DTARGET_APPLE")

	# Find the paths of all resources.
	file(GLOB_RECURSE resources RELATIVE ${PROJECT_SOURCE_DIR} data/*)

	# Make sure the resources are copied to the right bundle.
	foreach(r ${resources})
		get_filename_component(r_abs ${r} ABSOLUTE)
		get_filename_component(r_dir ${r_abs} PATH)
		file(RELATIVE_PATH r_rel ${PROJECT_SOURCE_DIR}/data ${r_dir})
		set_source_files_properties(${r} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/${r_rel})
	endforeach()

	# Executable.
	add_executable(
		${EXECUTABLE_NAME} MACOSX_BUNDLE
		${sources}
		${resources}
	)

# other systems
else ()
	add_executable(
		${EXECUTABLE_NAME}
		${sources}
	)
	target_compile_options(${EXECUTABLE_NAME} PRIVATE
		$<$<CXX_COMPILER_ID:MSVC>:/wd4061 /wd4062>          # enum not handled in switch
		$<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>>:-Wno-switch>
	)
endif ()


###  PACKAGES AND LIBRARIES
###  ======================

# Tell CMake to also search the thirdparty directory for modules.
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/thirdparty" ${CMAKE_MODULE_PATH})

set(SFML_STATIC_LIBRARIES OFF CACHE BOOL "")
find_package(SFML 3.0 CONFIG COMPONENTS Audio Graphics Window System)
if(NOT SFML_FOUND)
	message(STATUS "SFML Shared not found, trying static libraries...")
	set(SFML_STATIC_LIBRARIES ON CACHE BOOL "" FORCE)
	find_package(SFML 3.0 CONFIG REQUIRED COMPONENTS Audio Graphics Window System)
endif()



# Copy the SFML frameworks into the application bundle on Mac OS X.
if (APPLE)
	# install(DIRECTORY ${SFML_LIBRARIES} DESTINATION ${CMAKE_BINARY_DIR}/${EXECUTABLE_NAME}.app/Contents/Frameworks)
endif ()

# Alias SFML 3 targets to SFML 2 lowercase names to satisfy TGUI's dependencies
if(TARGET SFML::Graphics AND NOT TARGET sfml-graphics)
	add_library(sfml-graphics ALIAS SFML::Graphics)
endif()
if(TARGET SFML::Window AND NOT TARGET sfml-window)
	add_library(sfml-window ALIAS SFML::Window)
endif()
if(TARGET SFML::System AND NOT TARGET sfml-system)
	add_library(sfml-system ALIAS SFML::System)
endif()
if(TARGET SFML::Audio AND NOT TARGET sfml-audio)
	add_library(sfml-audio ALIAS SFML::Audio)
endif()

# TGUI
if(EXISTS "/home/tim/Projects/build/TGUI/build")
	set(TGUI_DIR "/home/tim/Projects/build/TGUI/build" CACHE PATH "Path to local TGUI build" FORCE)
	set(TGUI_STATIC_LIBRARIES TRUE)
else()
	set(TGUI_STATIC_LIBRARIES FALSE)
endif()

if(WIN32)
find_package(TGUI CONFIG REQUIRED)
else ()
find_package(TGUI 1 REQUIRED)
endif ()

# tinyxml2 (from vcpkg)
find_package(tinyxml2 CONFIG REQUIRED)

# Freetype (Needed by TGUI)
find_package(Freetype REQUIRED)

# OpenGL (Needed by Rocket)
find_package(OpenGL REQUIRED)

# libmspack for decompressing KWAJ files (SIMTOWER.EX_)
# http://www.cabextract.org.uk/libmspack/
find_package(mspack)
if (MSPACK_FOUND)
	include_directories(${MSPACK_INCLUDE_DIRS})
	link_directories(${MSPACK_LIBRARY_DIRS})
	target_link_libraries(${EXECUTABLE_NAME} PRIVATE ${MSPACK_LIBRARIES})
	add_definitions(-DMSPACK)
endif (MSPACK_FOUND)

#################################
# Generate Docs
#################################
OPTION(BUILD_WITH_DOCS "Generate Docs" ON)
IF(BUILD_WITH_DOCS)
find_package(Doxygen)
if (DOXYGEN_FOUND)
	message(STATUS "Doxygen found: ${DOXYGEN_EXECUTABLE} -- ${DOXYGEN_VERSION}")
	# Set Doxygen input and output files.
	set(DOXYGEN_INPUT_DIR ${PROJECT_SOURCE_DIR}/source)
	set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
	set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/doc/source/Doxyfile)
	set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
	# Generate DoxyFile from the input file.
	configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)
	# Command for generating doc from Doxygen config file.
	add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
					MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
					COMMENT "Generating Doxygen documentation"
					VERBATIM)
	# Create CMake Target for generating doc.
	add_custom_target(docs ALL DEPENDS ${DOXYGEN_INDEX_FILE})
endif (DOXYGEN_FOUND)
ENDIF()


# Link using keyword sig (avoid CMP0023 warning)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE
  SFML::Audio
  SFML::Graphics
  SFML::Window
  SFML::System
  TGUI::TGUI
  tinyxml2::tinyxml2
  Freetype::Freetype
  OpenGL::GL
)

if(WIN32)
  target_link_libraries(${EXECUTABLE_NAME} PRIVATE Comdlg32)
endif()

###  TESTS
###  ======
# The test target compiles the pure-logic seam files directly. We avoid
# linking the full game library because it contains static SFML objects
# whose construction hangs outside a graphical context. This keeps the
# seam tests focused and fast.
add_executable(OpenSkyTests
  tests/test_main.cpp
  source/Time.cpp
  source/Route.cpp
  source/WindowsPEExecutable.cpp
  source/Path.cpp
  source/Logger.cpp
  source/NameManager.cpp
  source/LevelUp.cpp
)
target_compile_definitions(OpenSkyTests PRIVATE OPEN_SKY_ROUTE_SEAM_ONLY)
# Lighting.h pulls in <SFML/Graphics/Color.hpp> for the tint color. We only
# need the headers (sf::Color is a POD struct defined inline), but linking
# against SFML::Graphics is the simplest way to make the include paths
# available to the test target without pulling in the rest of the engine.
target_link_libraries(OpenSkyTests PRIVATE SFML::Graphics)
add_test(NAME OpenSkyTests COMMAND OpenSkyTests)

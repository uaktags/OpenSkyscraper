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

# Gather the source files (includes batch5 Housekeeping.cpp and ParkingRamp.cpp)
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
	set(TGUI_STATIC_LIBRARIES FALSE CACHE BOOL "")
endif()

if(WIN32)
find_package(TGUI CONFIG)
else ()
find_package(TGUI 1)
endif ()
if(NOT TGUI_FOUND)
	message(STATUS "TGUI Shared not found, trying static libraries...")
	set(TGUI_STATIC_LIBRARIES TRUE CACHE BOOL "" FORCE)
	if(WIN32)
		find_package(TGUI CONFIG REQUIRED)
	else ()
		find_package(TGUI 1 REQUIRED)
	endif ()
endif()

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
  source/BuildRules.cpp
  source/YootPool.cpp
  source/YootWorld.cpp
  source/YootEvents.cpp
  source/YootCatalog.cpp
  source/YootTenants.cpp
  source/YootRules.cpp
  source/YootBackdrop.cpp
  source/YootFacilities.cpp
  source/YootVenues.cpp
  source/YootMovie.cpp
  source/YootPlacement.cpp
  source/YootSpecials.cpp
  source/YootMisc.cpp
  source/Skin.cpp
  source/Time.cpp
  source/Route.cpp
  source/SimFlag.cpp
  source/SaveHash.cpp
  source/WindowsPEExecutable.cpp
  source/Path.cpp
  source/Logger.cpp
  source/NameManager.cpp
  source/TdtLoader.cpp
  source/BldLoader.cpp
  source/BldExport.cpp
  source/LevelUp.cpp
  source/PluginManager.cpp
  source/PluginResources.cpp
  source/PluginDefs.cpp
  source/SubPluginDefs.cpp
  source/PluginResourceCodec.cpp
  source/PluginObjmap.cpp
  source/PluginBitmapLoader.cpp
  source/OspPlugin.cpp
  source/ResourceJson.cpp
  source/WorldResourceCodec.cpp
  source/PluginResourceJson.cpp
  source/PluginT2pExport.cpp
  source/AbstractPlugin.cpp
  source/DecompiledPlugin.cpp
)
target_compile_definitions(OpenSkyTests PRIVATE OPEN_SKY_ROUTE_SEAM_ONLY)
# Lighting.h pulls in <SFML/Graphics/Color.hpp> for the tint color. We only
# need the headers (sf::Color is a POD struct defined inline), but linking
# against SFML::Graphics is the simplest way to make the include paths
# available to the test target without pulling in the rest of the engine.
target_link_libraries(OpenSkyTests PRIVATE SFML::Graphics tinyxml2::tinyxml2)
# The full engine has global SFML textures that require a display before main.
# Keep it in a separate process, invoked by the seam runner only with graphics.
set(test_engine_sources ${sources})
list(FILTER test_engine_sources EXCLUDE REGEX "/main\\.cpp$")
add_executable(OpenSkyTransportTests tests/test_main.cpp ${test_engine_sources})
target_include_directories(OpenSkyTransportTests PRIVATE source)
target_compile_definitions(OpenSkyTransportTests PRIVATE
  OPENSKY_TRANSPORT_SCENARIOS OPENSKY_TRANSPORT_ONLY
)
target_link_libraries(OpenSkyTransportTests PRIVATE
  SFML::Audio SFML::Graphics SFML::Window SFML::System
  TGUI::TGUI tinyxml2::tinyxml2 Freetype::Freetype OpenGL::GL
)
if(MSPACK_FOUND)
  target_link_libraries(OpenSkyTransportTests PRIVATE ${MSPACK_LIBRARIES})
endif()
if(WIN32)
  target_link_libraries(OpenSkyTransportTests PRIVATE Comdlg32)
endif()
add_dependencies(OpenSkyTests OpenSkyTransportTests)
# Disaster/economy scenario harness (opt-in define, same engine link).
add_executable(OpenSkyDisasterTests tests/test_main.cpp ${test_engine_sources})
target_include_directories(OpenSkyDisasterTests PRIVATE source)
target_compile_definitions(OpenSkyDisasterTests PRIVATE OPENSKY_DISASTER_SCENARIOS)
target_link_libraries(OpenSkyDisasterTests PRIVATE
  SFML::Audio SFML::Graphics SFML::Window SFML::System
  TGUI::TGUI tinyxml2::tinyxml2 Freetype::Freetype OpenGL::GL
)
if(MSPACK_FOUND)
  target_link_libraries(OpenSkyDisasterTests PRIVATE ${MSPACK_LIBRARIES})
endif()
if(WIN32)
  target_link_libraries(OpenSkyDisasterTests PRIVATE Comdlg32)
endif()
target_compile_definitions(OpenSkyTests PRIVATE
  OPENSKY_TRANSPORT_TEST_EXECUTABLE="$<TARGET_FILE:OpenSkyTransportTests>"
)
add_test(NAME OpenSkyTests COMMAND OpenSkyTests)

# BldTool: command-line YootTower .bld summary / round trip / import /
# export over the same pure seam (tools/yoottower/bldtool.cpp).
add_executable(BldTool
  tools/yoottower/bldtool.cpp
  source/YootPool.cpp
  source/BldLoader.cpp
  source/BldExport.cpp
  source/YootWorld.cpp
  source/YootTenants.cpp
  source/YootCatalog.cpp
  source/Path.cpp
  source/Logger.cpp
  source/WindowsPEExecutable.cpp
  source/PluginManager.cpp
  source/PluginResources.cpp
  source/PluginDefs.cpp
  source/SubPluginDefs.cpp
  source/PluginResourceCodec.cpp
  source/PluginObjmap.cpp
  source/PluginBitmapLoader.cpp
  source/OspPlugin.cpp
  source/ResourceJson.cpp
  source/WorldResourceCodec.cpp
  source/PluginResourceJson.cpp
  source/PluginT2pExport.cpp
  source/AbstractPlugin.cpp
  source/DecompiledPlugin.cpp
)
target_include_directories(BldTool PRIVATE source)
target_link_libraries(BldTool PRIVATE SFML::Graphics tinyxml2::tinyxml2)

# Headless deterministic sim goldens (doc/headless-sim.md): a fixed-tick
# --sim run per ruleset whose JSON summary must match tests/sim/*.json.
# Regenerate with: cmake -DUPDATE_GOLDEN=ON ... -P tests/sim/run_sim_golden.cmake
foreach(sim_ruleset simtower opensky)
  foreach(sim_case default new)
    if(sim_case STREQUAL "new")
      set(sim_tower new)
    else()
      set(sim_tower ${PROJECT_SOURCE_DIR}/data/default.tower)
    endif()
    set(sim_name sim_${sim_case}_${sim_ruleset})
    add_test(NAME ${sim_name}
      COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:${EXECUTABLE_NAME}>
        -DTOWER=${sim_tower}
        -DRULESET=${sim_ruleset}
        -DTICKS=2000
        -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/sim/${sim_name}.json
        -DOUT_DIR=${CMAKE_BINARY_DIR}/sim-golden
        -P ${PROJECT_SOURCE_DIR}/tests/sim/run_sim_golden.cmake)
    set_tests_properties(${sim_name} PROPERTIES
      SKIP_REGULAR_EXPRESSION "SKIP_NO_DISPLAY" TIMEOUT 600)
  endforeach()
endforeach()

# Hotel coverage golden: a tower with hotel rooms (single/double/suite)
# plus office/condo/shop/restaurant/fastfood, an elevator and stairs, so
# the hotel save path (no yoot* attributes under these rulesets) and the
# touched item types are exercised on every ruleset save.
foreach(sim_ruleset simtower opensky)
  set(sim_name sim_hotel_${sim_ruleset})
  add_test(NAME ${sim_name}
    COMMAND ${CMAKE_COMMAND}
      -DEXE=$<TARGET_FILE:${EXECUTABLE_NAME}>
      -DTOWER=${PROJECT_SOURCE_DIR}/tests/sim/hotel_tower.tower
      -DRULESET=${sim_ruleset}
      -DTICKS=2000
      -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/sim/${sim_name}.json
      -DOUT_DIR=${CMAKE_BINARY_DIR}/sim-golden
      -P ${PROJECT_SOURCE_DIR}/tests/sim/run_sim_golden.cmake)
  set_tests_properties(${sim_name} PROPERTIES
    SKIP_REGULAR_EXPRESSION "SKIP_NO_DISPLAY" TIMEOUT 600)
endforeach()

# YootTower people (plan step 15): the visitor pool and walking customers
# of a small Tokyo tower (tests/sim/yoot_people.tower, plugins from data/).
add_test(NAME sim_people_yoottower
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:${EXECUTABLE_NAME}>
    -DTOWER=${PROJECT_SOURCE_DIR}/tests/sim/yoot_people.tower
    -DRULESET=yoottower
    -DTICKS=2000
    -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/sim/sim_people_yoottower.json
    -DOUT_DIR=${CMAKE_BINARY_DIR}/sim-golden
    -P ${PROJECT_SOURCE_DIR}/tests/sim/run_sim_golden.cmake)
set_tests_properties(sim_people_yoottower PROPERTIES
  SKIP_REGULAR_EXPRESSION "SKIP_NO_DISPLAY" TIMEOUT 600)

# YootTower visitors that walk to venues, hotel rooms and the Front Desk
# (tests/sim/yoot_visitors.tower: Classic Theater, Event Center, Gathering
# Area, Ticket Booth, Front Desk, rooms, Housekeeping on the people tower).
add_test(NAME sim_visitors_yoottower
  COMMAND ${CMAKE_COMMAND}
    -DEXE=$<TARGET_FILE:${EXECUTABLE_NAME}>
    -DTOWER=${PROJECT_SOURCE_DIR}/tests/sim/yoot_visitors.tower
    -DRULESET=yoottower
    -DTICKS=2000
    -DGOLDEN=${PROJECT_SOURCE_DIR}/tests/sim/sim_visitors_yoottower.json
    -DOUT_DIR=${CMAKE_BINARY_DIR}/sim-golden
    -P ${PROJECT_SOURCE_DIR}/tests/sim/run_sim_golden.cmake)
set_tests_properties(sim_visitors_yoottower PROPERTIES
  SKIP_REGULAR_EXPRESSION "SKIP_NO_DISPLAY" TIMEOUT 600)

# ---------------------------------------------------------------------------
# PluginEditor — standalone editor for YootTower plugins and the OSP format.
# Links the same SFML 3 + TGUI stack as the game plus the pure plugin seam
# (mirroring OpenSkyTests' isolation of those files). New editor code lives
# under tools/PluginEditor/. Edits every resource of every YootTower plugin
# class, saves OSP v3 and exports .t2p.
# ---------------------------------------------------------------------------
add_executable(PluginEditor
  tools/PluginEditor/src/main.cpp
  tools/PluginEditor/src/EditorApp.cpp
  tools/PluginEditor/src/EditorTheme.cpp
  tools/PluginEditor/src/FieldSchema.cpp
  tools/PluginEditor/src/ResourceForm.cpp
  source/Path.cpp
  source/Logger.cpp
  source/WindowsPEExecutable.cpp
  source/PluginManager.cpp
  source/PluginResources.cpp
  source/PluginDefs.cpp
  source/SubPluginDefs.cpp
  source/PluginResourceCodec.cpp
  source/PluginObjmap.cpp
  source/PluginBitmapLoader.cpp
  source/OspPlugin.cpp
  source/ResourceJson.cpp
  source/WorldResourceCodec.cpp
  source/PluginResourceJson.cpp
  source/PluginT2pExport.cpp
  source/AbstractPlugin.cpp
  source/DecompiledPlugin.cpp
  source/NativeFileDialog.cpp
)
target_include_directories(PluginEditor PRIVATE source)
target_link_libraries(PluginEditor PRIVATE
  SFML::Graphics SFML::Window SFML::System SFML::Audio
  TGUI::TGUI
)


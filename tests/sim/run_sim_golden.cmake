# Headless deterministic sim regression test (doc/headless-sim.md).
#
# Runs OpenSkyscraper --sim on a tower for a fixed number of ticks and
# compares the JSON summary with a checked-in golden file.
#
#   cmake -DEXE=<OpenSkyscraper> -DTOWER=<tower|new> -DRULESET=<id>
#         -DTICKS=<n> -DGOLDEN=<file.json> -DOUT_DIR=<dir>
#         [-DUPDATE_GOLDEN=ON] -P run_sim_golden.cmake
#
# The game needs an X display for its (hidden) window: an existing
# DISPLAY is used, otherwise xvfb-run. Without either the test reports
# SKIP_NO_DISPLAY, which CTest maps to "skipped".

foreach(var EXE TOWER RULESET TICKS GOLDEN OUT_DIR)
	if(NOT DEFINED ${var})
		message(FATAL_ERROR "run_sim_golden.cmake: ${var} is required")
	endif()
endforeach()

get_filename_component(name "${GOLDEN}" NAME_WE)
file(MAKE_DIRECTORY "${OUT_DIR}")
set(out "${OUT_DIR}/${name}.tower")
file(REMOVE "${out}" "${out}.json")

set(launcher)
if("$ENV{DISPLAY}" STREQUAL "")
	find_program(XVFB_RUN xvfb-run)
	if(NOT XVFB_RUN)
		message("SKIP_NO_DISPLAY: no DISPLAY and no xvfb-run for ${name}")
		return()
	endif()
	set(launcher ${XVFB_RUN} -a)
endif()

if(TOWER STREQUAL "new")
	set(load --new-game)
else()
	set(load --load-game "${TOWER}")
endif()

get_filename_component(exe_dir "${EXE}" DIRECTORY)
execute_process(
	COMMAND ${launcher} "${EXE}" --ruleset ${RULESET} ${load}
	        --sim ${TICKS} --sim-out "${out}"
	WORKING_DIRECTORY "${exe_dir}"
	RESULT_VARIABLE rc
	OUTPUT_FILE "${out}.log"
	ERROR_FILE "${out}.log")
if(NOT rc EQUAL 0 OR NOT EXISTS "${out}.json")
	message(FATAL_ERROR "sim run failed (exit ${rc}); see ${out}.log")
endif()

# SimTower/OpenSky saves must keep the pre-YootTower format: no world or
# yoot* attributes.
if(NOT RULESET STREQUAL "yoottower")
	file(READ "${out}" save)
	string(REGEX MATCH "[ \t\n](world|yoot[A-Za-z]*)=\"[^\"]*\"" leaked "${save}")
	if(leaked)
		message(FATAL_ERROR "${RULESET} save contains YootTower state: ${leaked}")
	endif()
endif()

if(UPDATE_GOLDEN)
	file(COPY_FILE "${out}.json" "${GOLDEN}")
	message("updated ${GOLDEN}")
	return()
endif()

file(READ "${out}.json" actual)
file(READ "${GOLDEN}" expected)
if(NOT actual STREQUAL expected)
	execute_process(COMMAND diff -u "${GOLDEN}" "${out}.json")
	message(FATAL_ERROR "sim summary differs from ${GOLDEN} (actual: ${out}.json)")
endif()
message("sim summary matches ${GOLDEN}")

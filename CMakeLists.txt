cmake_minimum_required(VERSION 3.15)

#! CHANGE YOUR PROJECT NAME
set(PROJECT_NAME ucutag-project)
set(EXECUTABLE_NAME ucutag)
project(${PROJECT_NAME}
		VERSION 1.0.0
		DESCRIPTION "Recursive ls"
		HOMEPAGE_URL "https://github.com/yewhenp/UCUTag"
		LANGUAGES C CXX) # project(${PROJECT_NAME} C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(WARNINGS_AS_ERRORS ON)
option(ENABLE_PVS_STUDIO "Check using command-line PVS-Studio." OFF)
option(ENABLE_SANITIZERS "Enable leak and memory error sanitizers" OFF)

## PVS Studio
if(ENABLE_PVS_STUDIO)
	include(${CMAKE_SOURCE_DIR}/cmake/extra/PVS-Studio.cmake)
	SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if (ENABLE_SANITIZERS)
	if (CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -fsanitize=undefined")
		message(STATUS "SANITIZERS ARE ENABLED")
		if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
			message(WARNING "Using sanitizer in Release build type")
		endif()
	endif()
endif()

#! Build release version if not specified otherwise.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif ()
if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
	add_compile_definitions(DEBUG=1)
endif()
message(STATUS "BUILD TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "USING PVS: ${ENABLE_PVS_STUDIO}")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(FUSE REQUIRED)

# REQUIRED FOR FUSE
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-D_FILE_OFFSET_BITS=64")

include_directories(include)
add_executable(${EXECUTABLE_NAME} src/tagfs_api.cpp src/TagFS.cpp include/TagFS.h include/string_utils.h src/string_utils.cpp include/typedefs.h src/typedefs.cpp include/tagfs_api.h)
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${FUSE_INCLUDE_DIR})
target_link_libraries(${EXECUTABLE_NAME} ${FUSE_LIBRARIES})
set_target_properties(${EXECUTABLE_NAME}
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set (ALL_TARGETS ${EXECUTABLE_NAME})

# Include fixed CMake configuration
include(cmake/config.cmake)

# add targets for pvs studio
if(ENABLE_PVS_STUDIO)
	# create own target for library
	pvs_studio_add_target(
			TARGET ${EXECUTABLE_NAME}.analyze ALL
			ANALYZE ${EXECUTABLE_NAME}
			OUTPUT FORMAT errorfile
			MODE GA:1,2,3 OP:1,2,3 GA:1,2,3 OP:1,2,3 64:1,2,3 CS:1,2,3 MISRA:1,2,3
			LOG target_${EXECUTABLE_NAME}.err)
else()
	message("Consider checking using PVS-Studio with `cmake -DENABLE_PVS_STUDIO=ON <.....>`")
endif()

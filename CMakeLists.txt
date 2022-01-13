cmake_minimum_required(VERSION 3.15)
set(PROJECT_NAME ucutag-project)
set(EXECUTABLE_NAME ucutag)
project(${PROJECT_NAME}
		VERSION 1.0.0
		DESCRIPTION "Fuse based tag-oriented filesystem"
		HOMEPAGE_URL "https://github.com/yewhenp/UCUTag"
		LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(WARNINGS_AS_ERRORS ON)


# options
option(ENABLE_PVS_STUDIO "Check using command-line PVS-Studio." OFF)
option(ENABLE_SANITIZERS "Enable leak and memory error sanitizers" OFF)


# PVS Studio
if(ENABLE_PVS_STUDIO)
	include(${CMAKE_SOURCE_DIR}/cmake/extra/PVS-Studio.cmake)
	SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()


# Compiler flags intervention
if (ENABLE_SANITIZERS)
	if (CMAKE_COMPILER_IS_GNUCXX)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -fsanitize=undefined")
		message(STATUS "SANITIZERS ARE ENABLED")
		if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
			message(WARNING "Using sanitizer in Release build type")
		endif()
	endif()
endif()


# REQUIRED FOR FUSE
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-D_FILE_OFFSET_BITS=64")


# build type
#! Build release version if not specified otherwise.
if (NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif ()
if (${CMAKE_BUILD_TYPE} STREQUAL Debug)
	add_compile_definitions(DEBUG=1)
endif()
message(STATUS "BUILD TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "USING PVS: ${ENABLE_PVS_STUDIO}")


# cmake modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)


# Required packages
find_package(FUSE REQUIRED)
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)
find_package(Boost REQUIRED)


# Includes
include_directories(${LIBMONGOCXX_INCLUDE_DIR})
include_directories(${LIBBSONCXX_INCLUDE_DIR})
include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)


# main executable
add_executable(${EXECUTABLE_NAME} 
				src/tagfs_api.cpp src/TagFS.cpp src/string_utils.cpp src/typedefs.cpp 
				
				include/TagFS.h include/string_utils.h include/tagfs_api.h include/typedefs.h )


# linking
target_include_directories(${EXECUTABLE_NAME} PRIVATE ${FUSE_INCLUDE_DIR})
target_link_libraries(${EXECUTABLE_NAME} ${FUSE_LIBRARIES} mongo::mongocxx_shared mongo::bsoncxx_shared ${Boost_LIBRARIES})


# properties
set_target_properties(${EXECUTABLE_NAME}
		PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set (ALL_TARGETS ${EXECUTABLE_NAME} test_mongo_db)


# Include fixed CMake configuration
include(cmake/config.cmake)


# Add targets for pvs studio
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

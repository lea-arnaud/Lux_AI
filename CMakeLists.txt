cmake_minimum_required( VERSION 3.0 )
project( 1 C CXX )

#add_definitions(-DDEBUG_FRAMEWORK)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
)

# Filters
include( Filters.cmake )


FILE(GLOB SOURCE
	 "*.hpp"
     "*.cpp"
     "lux/*.hpp"
     "lux/*.cpp"
)

SET( AIBOT_BUILDFILES 
	
	CleanAll.bat
	CMakeLists.txt
	Filters.cmake
	GenSubmit.bat
	GenSubmit.py
	PlayLocalMatch.bat
	PlayLocalMatch.py
	rebuild_vis_sln_x64.bat
)

SET( AIBOT_HEADERS 

	#Framework
	lux/agent.hpp
	lux/kit.hpp
	log.h
	Bot.h
	City.h
	Map.h
	Tile.h
	command_chain.h
	game_state.h
	turn_order.h
	types.h
)

SET( AIBOT_SRC 

	#Framework
	lux/agent.cpp
	Bot.cpp
	City.cpp
	Map.cpp
	Tile.cpp
	main.cpp
	command_chain.cpp
	game_state.cpp
	turn_order.cpp
)

SET(jobfiles "${AIBOT_HEADERS};${AIBOT_SRC};${AIBOT_BUILDFILES}")
file(WRITE jobfiles.txt "${jobfiles}")


add_executable(${PROJECT_NAME} ${AIBOT_SRC} ${AIBOT_HEADERS})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_SUPPRESS_REGENERATION true)

target_include_directories (${PROJECT_NAME} PUBLIC ${AIBOT_HEADERS})


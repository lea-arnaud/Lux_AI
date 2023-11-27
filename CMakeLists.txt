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
	lux/annotate.hpp
	Log.h
	Bot.h
	City.h
	Map.h
	Tile.h
	CommandChain.h
	GameState.h
	TurnOrder.h
	Types.h
	BehaviorTree.h
	BehaviorTreeNodes.h
	BehaviorTreeNames.h
	Pathing.h
	GameRules.h
	AStar.h
	InfluenceMap.h
	IAParameters.h
	Statistics.h
	Benchmarking.h
)

SET( AIBOT_SRC 

	#Framework
	lux/agent.cpp
	Bot.cpp
	City.cpp
	Map.cpp
	Tile.cpp
	main.cpp
	CommandChain.cpp
	BehaviorTreeNodes.cpp
	Pathing.cpp
	GameState.cpp
	TurnOrder.cpp
	InfluenceMap.cpp
	Benchmarking.cpp
	IAParams.cpp
)

SET(jobfiles "${AIBOT_HEADERS};${AIBOT_SRC};${AIBOT_BUILDFILES}")
file(WRITE jobfiles.txt "${jobfiles}")


add_executable(${PROJECT_NAME} ${AIBOT_SRC} ${AIBOT_HEADERS})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set(CMAKE_SUPPRESS_REGENERATION true)

target_include_directories (${PROJECT_NAME} PUBLIC ${AIBOT_HEADERS})


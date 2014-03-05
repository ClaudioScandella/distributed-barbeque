
#----- Add "Random" target dynamic library
if (NOT CONFIG_BBQUE_SCHEDPOL_RANDOM)
	return(random)
endif(NOT CONFIG_BBQUE_SCHEDPOL_RANDOM)

# Set the macro for the scheduling policy loading
if (CONFIG_BBQUE_SCHEDPOL_DEFAULT_RANDOM)
  set (BBQUE_SCHEDPOL_DEFAULT "random" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_BBQUE_SCHEDPOL_DEFAULT_RANDOM)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(PLUGIN_RANDOM_SRC  random_schedpol random_plugin)
  add_library(bbque_schedpol_random MODULE ${PLUGIN_RANDOM_SRC})
  target_link_libraries(
	bbque_schedpol_random
	${Boost_LIBRARIES}
  )
  install(TARGETS bbque_schedpol_random LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS})
endif (CMAKE_BUILD_TYPE STREQUAL "Debug")

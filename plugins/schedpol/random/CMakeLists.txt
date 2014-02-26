
#----- Add "Random" target dynamic library
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

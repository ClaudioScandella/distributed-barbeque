
#----- Add "YaMS" target dynamic library
if (NOT CONFIG_BBQUE_SCHEDPOL_YAMS)
  return(yams)
endif(NOT CONFIG_BBQUE_SCHEDPOL_YAMS)

# Set the macro for the scheduling policy loading
if (CONFIG_BBQUE_SCHEDPOL_DEFAULT_YAMS)
  set (BBQUE_SCHEDPOL_DEFAULT "yams" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_BBQUE_SCHEDPOL_DEFAULT_YAMS)

add_subdirectory(contrib)

set(PLUGIN_YAMS_SRC  yams_schedpol yams_plugin)
add_library(bbque_schedpol_yams MODULE ${PLUGIN_YAMS_SRC})
target_link_libraries(
	bbque_schedpol_yams
	bbque_sched_contribs
	${Boost_LIBRARIES}
)

install(TARGETS bbque_schedpol_yams LIBRARY
	DESTINATION ${BBQUE_PATH_PLUGINS}
	COMPONENT BarbequeRTRM)


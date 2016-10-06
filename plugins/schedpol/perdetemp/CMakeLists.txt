
#----- Add "PERDETEMP" target dynamic library

if (NOT CONFIG_BBQUE_SCHEDPOL_PERDETEMP)
	return(perdetemp)
endif(NOT CONFIG_BBQUE_SCHEDPOL_PERDETEMP)

# Set the macro for the scheduling policy loading
if (CONFIG_SCHEDPOL_DEFAULT_PERDETEMP)
  set (BBQUE_SCHEDPOL_DEFAULT "perdetemp" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_SCHEDPOL_DEFAULT_PERDETEMP)

set(PLUGIN_PERDETEMP_SRC perdetemp_schedpol perdetemp_plugin)

add_library(bbque_schedpol_perdetemp MODULE ${PLUGIN_PERDETEMP_SRC})

target_link_libraries(
	bbque_schedpol_perdetemp
	${Boost_LIBRARIES}
)

install(TARGETS bbque_schedpol_perdetemp LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

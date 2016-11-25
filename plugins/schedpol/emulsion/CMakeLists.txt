
#----- Add "EMULSION" target dynamic library

if (NOT CONFIG_BBQUE_RT_SCHEDPOL_EMULSION)
	return(emulsion)
endif(NOT CONFIG_BBQUE_RT_SCHEDPOL_EMULSION)

# Set the macro for the scheduling policy loading
if (CONFIG_BBQUE_RT_SCHEDPOL_DEFAULT_EMULSION)
  set (BBQUE_RT_SCHEDPOL_DEFAULT "emulsion" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_BBQUE_RT_SCHEDPOL_DEFAULT_EMULSION)

set(PLUGIN_EMULSION_SRC emulsion_schedpol emulsion_plugin)

add_library(bbque_schedpol_emulsion MODULE ${PLUGIN_EMULSION_SRC})

target_link_libraries(
	bbque_schedpol_emulsion
	${Boost_LIBRARIES}
)

install(TARGETS bbque_schedpol_emulsion LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

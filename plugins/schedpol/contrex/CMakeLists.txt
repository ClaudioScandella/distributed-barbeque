
#----- Add "CONTREX" target dynamic library

if (NOT CONFIG_BBQUE_SCHEDPOL_CONTREX)
	return(contrex)
endif(NOT CONFIG_BBQUE_SCHEDPOL_CONTREX)

# Set the macro for the scheduling policy loading
if (CONFIG_SCHEDPOL_DEFAULT_CONTREX)
  set (BBQUE_SCHEDPOL_DEFAULT "contrex" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_SCHEDPOL_DEFAULT_CONTREX)

set(PLUGIN_CONTREX_SRC contrex_schedpol contrex_plugin)

add_library(bbque_schedpol_contrex MODULE ${PLUGIN_CONTREX_SRC})

target_link_libraries(
	bbque_schedpol_contrex
	${Boost_LIBRARIES}
)

install(TARGETS bbque_schedpol_contrex LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

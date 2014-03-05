
#----- Add "YaMCA" target dynamic library
if (NOT CONFIG_BBQUE_SCHEDPOL_YAMCA)
  return(yamca)
endif(NOT CONFIG_BBQUE_SCHEDPOL_YAMCA)

# Set the macro for the scheduling policy loading
if (CONFIG_BBQUE_SCHEDPOL_DEFAULT_YAMCA)
  set (BBQUE_SCHEDPOL_DEFAULT "yamca" CACHE STRING
	  "Setting scheduling policy name" FORCE)
endif (CONFIG_BBQUE_SCHEDPOL_DEFAULT_YAMCA)

set(PLUGIN_YAMCA_SRC  yamca_schedpol yamca_plugin)
add_library(bbque_schedpol_yamca MODULE ${PLUGIN_YAMCA_SRC})
target_link_libraries(
	bbque_schedpol_yamca
	${Boost_LIBRARIES}
)

install(TARGETS bbque_schedpol_yamca LIBRARY
	DESTINATION ${BBQUE_PATH_PLUGINS}
	COMPONENT BarbequeRTRM)


#----- Add "YaMCA" target dynamic library
set(PLUGIN_YAMCA_SRC  yamca_schedpol yamca_plugin)
add_library(bbque_schedpol_yamca MODULE ${PLUGIN_YAMCA_SRC})
target_link_libraries(
	bbque_schedpol_yamca
	${Boost_LIBRARIES}
)
install(TARGETS bbque_schedpol_yamca LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

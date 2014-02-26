
#----- Add "YaMS" target dynamic library
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



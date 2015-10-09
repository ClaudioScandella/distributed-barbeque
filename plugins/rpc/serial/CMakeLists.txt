
if (NOT CONFIG_BBQUE_RPC_SERIAL)
	return(serial)
endif(NOT CONFIG_BBQUE_RPC_SERIAL)


# Set the macro for the RPC plugin loading
if (CONFIG_BBQUE_RPC_DEFAULT_SERIAL)
  set (BBQUE_RPC_DEFAULT "serial" CACHE STRING "Setting default recipe loader" FORCE)
endif (CONFIG_BBQUE_RPC_DEFAULT_SERIAL)

# Sources
set(PLUGIN_SERIAL_SRC serial_rpc serial_plugin)

add_library(bbque_rpc_serial MODULE ${PLUGIN_SERIAL_SRC})

target_link_libraries(
	bbque_rpc_serial
        ${Boost_LIBRARIES}
)

install(TARGETS bbque_rpc_serial LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

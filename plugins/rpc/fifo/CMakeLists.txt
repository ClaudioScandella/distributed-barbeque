
#----- Add "RPC FIFO" target dynamic library
set(PLUGIN_RPC_FIFO_SRC  fifo_rpc fifo_plugin)

# Set the macro for the RPC plugin loading
if (CONFIG_BBQUE_RPC_DEFAULT_FIFO)
  set (BBQUE_RPC_DEFAULT "fifo" CACHE STRING "Setting default recipe loader" FORCE)
endif (CONFIG_BBQUE_RPC_DEFAULT_FIFO)

add_library(bbque_rpc_fifo MODULE ${PLUGIN_RPC_FIFO_SRC})
target_link_libraries(
	bbque_rpc_fifo
	${Boost_LIBRARIES}
)
install(TARGETS bbque_rpc_fifo LIBRARY
		DESTINATION ${BBQUE_PATH_PLUGINS}
		COMPONENT BarbequeRTRM)

#----- Add "RPC FIFO" specific flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")



## Add package version definition
configure_file (
	"${PROJECT_SOURCE_DIR}/bbque/version.cc.in"
	"${PROJECT_BINARY_DIR}/bbque/version.cc" @ONLY
)

## Copy the AEM header file where it is expected by AS-RTRM
configure_file (
	"${PROJECT_SOURCE_DIR}/include/bbque/rtlib/bbque_exc.h"
	"${PROJECT_BINARY_DIR}/include/bbque/bbque_exc.h" COPYONLY
)

## Copy the RT AEM header file where it is expected by AS-RTRM
configure_file (
	"${PROJECT_SOURCE_DIR}/include/bbque/rtlib/bbque_rtexc.h"
	"${PROJECT_BINARY_DIR}/include/bbque/bbque_rtexc.h" COPYONLY
)

# Add "barbeque" specific flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBBQUE_RTLIB")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")

if (CONFIG_BBQUE_OPENCL)
	include_directories(${OPENCL_INCLUDE_DIR})
endif (CONFIG_BBQUE_OPENCL)

set (RTLIB_SRC bbque_rtlib bbque_rpc ${PROJECT_BINARY_DIR}/bbque/version.cc)
set (RTLIB_SRC bbque_exc ${RTLIB_SRC})

if (CONFIG_BBQUE_RT)
	set (RTLIB_SRC ${RTLIB_SRC} bbque_rtexc)
endif (CONFIG_BBQUE_RT)

if (CONFIG_BBQUE_OPENCL)
	set (RTLIB_SRC bbque_ocl ${RTLIB_SRC})
endif (CONFIG_BBQUE_OPENCL)

if (CONFIG_BBQUE_RPC_FIFO)
	set (RTLIB_SRC rpc_fifo_client ${RTLIB_SRC})
endif (CONFIG_BBQUE_RPC_FIFO)

# Monitoring library subdirectory
if (CONFIG_BBQUE_RTLIB_MONITORS)
	add_subdirectory(monitors)
endif (CONFIG_BBQUE_RTLIB_MONITORS)

#----- Add "bbque_rtlib" target dinamic binary
add_library(bbque_rtlib SHARED ${RTLIB_SRC})

# Linking dependencies
target_link_libraries(bbque_rtlib
	bbque_utils
	${Boost_LIBRARIES}
)

# Add "Generic-Linux" specific flags
if (CONFIG_TARGET_LINUX)
target_link_libraries(bbque_rtlib
	-ldl -lrt
)
endif (CONFIG_TARGET_LINUX)

# Linking dependencies for not Android-Linux targets
if (NOT CONFIG_TARGET_ANDROID)
  target_link_libraries(bbque_rtlib pthread)
endif (NOT CONFIG_TARGET_ANDROID)


# Use link path ad RPATH
set_property (TARGET bbque_rtlib PROPERTY
		INSTALL_RPATH_USE_LINK_PATH TRUE)

set_property (TARGET bbque_rtlib PROPERTY
		FRAMEWORK ON)

set_property (TARGET bbque_rtlib PROPERTY PUBLIC_HEADER
		${PROJECT_SOURCE_DIR}/include/bbque/rtlib.h
		${PROJECT_SOURCE_DIR}/include/bbque/rtlib/bbque_exc.h
		${PROJECT_SOURCE_DIR}/include/bbque/rtlib/bbque_rtexc.h)

# Install the RTLib
install (TARGETS bbque_rtlib LIBRARY
		DESTINATION ${BBQUE_PATH_RTLIB}
		PUBLIC_HEADER DESTINATION ${BBQUE_PATH_HEADERS}
		COMPONENT BarbequeRTLib)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/bbque/utils"
		DESTINATION "${BBQUE_PATH_HEADERS}"
		FILES_MATCHING PATTERN "*.h")

install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/bbque/cpp11"
		DESTINATION "${BBQUE_PATH_HEADERS}"
		FILES_MATCHING PATTERN "*.h")

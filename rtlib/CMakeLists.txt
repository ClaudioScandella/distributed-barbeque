
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

# Add "barbeque" specific flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBBQUE_RTLIB")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")

# Build subdirs
add_subdirectory(monitors)


################################################################################
# Library Sources
################################################################################

set (RTLIB_SRC bbque_rtlib bbque_rpc ${PROJECT_BINARY_DIR}/bbque/version.cc)
set (RTLIB_SRC bbque_exc ${RTLIB_SRC})

# Add FIFO RPC Channel (if configured)
if (CONFIG_BBQUE_RPC_FIFO)
	set (RTLIB_SRC rpc_fifo_client ${RTLIB_SRC})
endif (CONFIG_BBQUE_RPC_FIFO)


################################################################################
# Library Dependencies
################################################################################

# Common linking dependencies
set (RTLIB_LIBS bbque_utils ${Boost_LIBRARIES})

# Add "Generic-Linux" specific linking dependencies
if (CONFIG_TARGET_LINUX)
	set (RTLIB_LIBS ${RTLIB_LIBS}
		-ldl -lrt
	)
endif (CONFIG_TARGET_LINUX)

# Add "Android-Linux" specific linking dependencies
if (CONFIG_TARGET_ANDROID)
	set (RTLIB_LIBS ${RTLIB_LIBS}
		-Wl,-whole-archive bbque_cpp11 -Wl,-no-whole-archive
	)
endif (CONFIG_TARGET_ANDROID)

################################################################################
# Target: RTLib+DMM Library (RTLib with DMM support)
################################################################################

if (CONFIG_BBQUE_RTLIB_DMM_SUPPORT)

#----- Add "bbque_rtlib_dmm" target dinamic binary
set (RTLIB_SRC ${RTLIB_SRC} bbque_dmm)
set (RTLIB_LIBS
	-Wl,-whole-archive libdmm_static.a -Wl,-no-whole-archive
	${RTLIB_LIBS}
)

endif (CONFIG_BBQUE_RTLIB_DMM_SUPPORT)

################################################################################
# Target: RTLib Library
################################################################################

#----- Add "bbque_rtlib" target dinamic binary
add_library(bbque_rtlib SHARED ${RTLIB_SRC})

# Linking dependencies
target_link_libraries(
	bbque_rtlib
	${RTLIB_LIBS}
)

# Use link path ad RPATH
set_property (TARGET bbque_rtlib PROPERTY
		INSTALL_RPATH_USE_LINK_PATH TRUE)

set_property (TARGET bbque_rtlib PROPERTY
		FRAMEWORK ON)

set_property (TARGET bbque_rtlib PROPERTY PUBLIC_HEADER
		${PROJECT_SOURCE_DIR}/include/bbque/rtlib.h
		${PROJECT_SOURCE_DIR}/include/bbque/rtlib/bbque_exc.h)

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


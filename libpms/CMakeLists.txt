
set (TARGET_NAME bbque_pms)
set (VERSION_STRING 1.0.0)
set (SOURCE exec_synchronizer app_controller)

configure_file (
	"${PROJECT_SOURCE_DIR}/libpms/config/libpms.conf.in"
	"${PROJECT_BINARY_DIR}/libpms/config/libpms.conf"
)

# Add "barbeque" specific flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBBQUE_PMSLIB")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")


add_library(${TARGET_NAME} SHARED ${SOURCE})

add_dependencies(${TARGET_NAME}
	bbque_tg
)

target_link_libraries(${TARGET_NAME}
        bbque_rtlib
        bbque_tg
	${Boost_LIBRARIES}
)


# Add "Generic-Linux" specific flags
if (CONFIG_TARGET_LINUX)
target_link_libraries(${TARGET_NAME}
	-ldl -lrt
)
endif (CONFIG_TARGET_LINUX)

# Linking dependencies for not Android-Linux targets
if (NOT CONFIG_TARGET_ANDROID)
  target_link_libraries(${TARGET_NAME} pthread)
endif (NOT CONFIG_TARGET_ANDROID)


# Use link path ad RPATH
set_property (TARGET ${TARGET_NAME} PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)
set_property (TARGET ${TARGET_NAME} PROPERTY FRAMEWORK ON)
set_property (TARGET ${TARGET_NAME} PROPERTY PUBLIC_HEADER
		${PROJECT_SOURCE_DIR}/include/pmsl/exec_synchronizer.h
		${PROJECT_SOURCE_DIR}/include/pmsl/app_controller.h)
set_property (TARGET ${TARGET_NAME} PROPERTY VERSION ${VERSION_STRING})

# Install the Programming Model Synchronization Library
install (TARGETS ${TARGET_NAME} LIBRARY
		DESTINATION ${BBQUE_PATH_RTLIB}
		PUBLIC_HEADER DESTINATION ${BBQUE_PATH_HEADERS}/pmsl
		COMPONENT BarbequeSynclib)

# Default recipes
install (FILES generic.recipe
		DESTINATION ${BBQUE_PATH_RECIPES})
install (FILES generic_manga.recipe
		DESTINATION ${BBQUE_PATH_RECIPES})

# Configuration file
install (FILES "${PROJECT_BINARY_DIR}/libpms/config/libpms.conf"
		DESTINATION ${BBQUE_PATH_CONF})

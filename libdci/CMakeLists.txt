# Sources
set (TARGET_NAME bbque_dci)
set (VERSION_STRING 1.0.0)
set (SOURCE data_client ../bbque/res/resource_type)

# Output: a shared library
add_library(${TARGET_NAME} SHARED ${SOURCE})

# Include
include_directories(
	#${PROJECT_BINARY_DIR}/include/dci
	${PROJECT_SOURCE_DIR}/include/dci
	${Boost_INCLUDE_DIR}
)

# Link to:
target_link_libraries(${TARGET_NAME}
	${Boost_LIBRARIES}
)

# Set the public headers to install
set_property (TARGET ${TARGET_NAME} PROPERTY PUBLIC_HEADER
	${PROJECT_SOURCE_DIR}/include/dci/data_client.h
	${PROJECT_SOURCE_DIR}/include/dci/types.h
	${PROJECT_SOURCE_DIR}/include/bbque/res/resource_type.h
)
set_property (TARGET ${TARGET_NAME} PROPERTY VERSION ${VERSION_STRING})

# Install the library
install (TARGETS ${TARGET_NAME} LIBRARY
	DESTINATION ${BBQUE_PATH_RTLIB}
	PUBLIC_HEADER DESTINATION ${BBQUE_PATH_HEADERS}/dci
)

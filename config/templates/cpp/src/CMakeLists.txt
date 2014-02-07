
#----- Check for the required RTLib library
find_package(BbqRTLib REQUIRED)

#----- Add compilation dependencies
include_directories(${BBQUE_RTLIB_INCLUDE_DIR})

#----- Add "myapp" target application
set(MYAPP_SRC version MyApp_exc MyApp_main)
add_executable(myapp ${MYAPP_SRC})

#----- Linking dependencies
target_link_libraries(
	myapp
	${Boost_LIBRARIES}
	${BBQUE_RTLIB_LIBRARY}
)

# Use link path ad RPATH
set_property(TARGET myapp PROPERTY
	INSTALL_RPATH_USE_LINK_PATH TRUE)

#----- Install the MyApp files
install (TARGETS myapp RUNTIME
	DESTINATION ${MYAPP_PATH_BINS})

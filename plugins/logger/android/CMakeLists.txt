################################################################################
# Android Logger
################################################################################

if (CONFIG_TARGET_ANDROID)

## Add compilation dependencies
#include_directories(${log4cpp_INCLUDE_DIRS})

## Add linking dependencies
#link_directories(${log4cpp_LIBRARY_DIRS})

#----- Add target static library
set(PLUGIN_ANDROID_SRC  android_logger android_plugin)
add_library(bbque_logger_android STATIC ${PLUGIN_ANDROID_SRC})

#----- Add library specific flags
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")

#----- Linking dependencies
target_link_libraries(bbque_logger_android "-llog")

endif (CONFIG_TARGET_ANDROID)


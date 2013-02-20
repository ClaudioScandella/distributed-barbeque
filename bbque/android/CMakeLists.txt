
# Add sources in the current directory to the target binary
set (BBQUE_ANDROID_SRC getline getdelim)

#Add as library
add_library(bbque_android STATIC ${BBQUE_ANDROID_SRC})



## Add package version definition
configure_file (
	"${PROJECT_SOURCE_DIR}/bbque/version.cc.in"
	"${PROJECT_BINARY_DIR}/bbque/version.cc" @ONLY
)

# Add "barbeque" specific flags
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=c++0x")
# This flags should be used only when a monolitic build profile will be defined
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--gc-sections")

# Generate configuration file
configure_file (
	"${PROJECT_SOURCE_DIR}/include/bbque/config.h.in"
	"${PROJECT_BINARY_DIR}/include/bbque/config.h"
)

# Setup configuration files based on installation paths
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbque.conf.in"
	"${PROJECT_BINARY_DIR}/config/bbque.conf"
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbque.conf_dbg.in"
	"${PROJECT_BINARY_DIR}/config/bbque.conf_dbg"
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/rtlib.conf.in"
	"${PROJECT_BINARY_DIR}/config/rtlib.conf"
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbque_2parma_demo2.conf.in"
	"${PROJECT_BINARY_DIR}/config/bbque_2parma_demo2.conf"
)

# Setup the daemon initialization script
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbqued.in"
	"${PROJECT_BINARY_DIR}/config/bbqued"
	@ONLY
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbqued.defaults.in"
	"${PROJECT_BINARY_DIR}/config/default/bbqued"
)

# Setup BOSP shell
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bosp_init.env.in"
	"${PROJECT_BINARY_DIR}/config/bosp_init.env"
	@ONLY
)

# Setup HARPA-OS shell
configure_file (
	"${PROJECT_SOURCE_DIR}/config/harpa_init.env.in"
	"${PROJECT_BINARY_DIR}/config/harpa_init.env"
	@ONLY
)

# Setup BPL generator
if (CONFIG_BBQUE_PIL_LEGACY)
	configure_file (
		"${PROJECT_SOURCE_DIR}/config/pil/generator.in"
		"${PROJECT_BINARY_DIR}/config/pil/bbque-pil-generator"
		@ONLY
	)
endif (CONFIG_BBQUE_PIL_LEGACY)
if (CONFIG_BBQUE_PIL_LOADER)
	configure_file (
		"${PROJECT_SOURCE_DIR}/config/pil/generator_xml.in"
		"${PROJECT_BINARY_DIR}/config/pil/bbque-pil-generator"
		@ONLY
	)
endif (CONFIG_BBQUE_PIL_LOADER)


# Setup Template generator
configure_file (
	"${PROJECT_SOURCE_DIR}/config/bbque-layapp.sh.in"
	"${PROJECT_SOURCE_DIR}/config/bbque-layapp"
	@ONLY
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/templates/cpp/.cproject.in"
	"${PROJECT_SOURCE_DIR}/config/templates/cpp/.cproject"
	@ONLY
)
configure_file (
	"${PROJECT_SOURCE_DIR}/config/templates/cpp/.project.in"
	"${PROJECT_SOURCE_DIR}/config/templates/cpp/.project"
	@ONLY
)


# Build subdirs
add_subdirectory(utils)
add_subdirectory(app)
add_subdirectory(res)
if (NOT CONFIG_TARGET_SUPPORT_CPP11)
add_subdirectory(cpp11)
endif (NOT CONFIG_TARGET_SUPPORT_CPP11)
if (CONFIG_TARGET_ANDROID)
  add_subdirectory(android)
endif (CONFIG_TARGET_ANDROID)
if (CONFIG_BBQUE_PM)
  add_subdirectory(pm)
endif (CONFIG_BBQUE_PM)
if (CONFIG_BBQUE_EM)
  add_subdirectory(em)
endif (CONFIG_BBQUE_EM)


# Add "barbeque" target binary
set (BARBEQUE_SRC barbeque version object configuration_manager)
set (BARBEQUE_SRC platform_services dynamic_library ${BARBEQUE_SRC})
set (BARBEQUE_SRC plugin_manager modules_factory ${BARBEQUE_SRC})
set (BARBEQUE_SRC resource_manager application_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC resource_accounter ${BARBEQUE_SRC})
set (BARBEQUE_SRC binding_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC application_proxy rpc_proxy rpc_messages ${BARBEQUE_SRC})
set (BARBEQUE_SRC command_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC signals_manager scheduler_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC synchronization_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC profile_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC daemonize ${BARBEQUE_SRC})
set (BARBEQUE_SRC platform_manager ${BARBEQUE_SRC})
set (BARBEQUE_SRC platform_proxy ${BARBEQUE_SRC})
set (BARBEQUE_SRC pp/local_platform_proxy ${BARBEQUE_SRC})
if (CONFIG_BBQUE_DIST_MODE)
	set (BARBEQUE_SRC pp/remote_platform_proxy ${BARBEQUE_SRC})
endif (CONFIG_BBQUE_DIST_MODE)

if (CONFIG_BBQUE_TEST_PLATFORM_DATA)
	set (BARBEQUE_SRC pp/test_platform_proxy ${BARBEQUE_SRC})
else (CONFIG_BBQUE_TEST_PLATFORM_DATA)
  if (CONFIG_TARGET_LINUX)
	set (BARBEQUE_SRC pp/linux_platform_proxy ${BARBEQUE_SRC})
  endif (CONFIG_TARGET_LINUX)
endif (CONFIG_BBQUE_TEST_PLATFORM_DATA)

if (CONFIG_BBQUE_LINUX_PROC_LISTENER)
	set (BARBEQUE_SRC pp/proc_listener ${BARBEQUE_SRC})
endif (CONFIG_BBQUE_LINUX_PROC_LISTENER)

if (CONFIG_BBQUE_OPENCL)
	set (BARBEQUE_SRC pp/opencl_platform_proxy ${BARBEQUE_SRC})
endif (CONFIG_BBQUE_OPENCL)

if (CONFIG_BBQUE_PM)
	set (BARBEQUE_SRC power_monitor ${BARBEQUE_SRC})
endif (CONFIG_BBQUE_PM)


add_executable (barbeque ${BARBEQUE_SRC})

# Linking dependencies
target_link_libraries(barbeque
	bbque_utils
	bbque_resources
	bbque_apps
	${Boost_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)


# Linking dependencies for Generic-Linux targets
if (CONFIG_TARGET_LINUX)
target_link_libraries(barbeque
	${CGroup_LIBRARIES}
	-ldl -lrt
)
endif (CONFIG_TARGET_LINUX)

# Linking dependencies for Android-Linux targets
if (CONFIG_TARGET_ANDROID)
target_link_libraries(barbeque
	bbque_android
)
endif (CONFIG_TARGET_ANDROID)

# Linking dependencyes for OpenCL support
if (CONFIG_BBQUE_OPENCL)
target_link_libraries(barbeque
	${OPENCL_LIBRARY}
)
endif (CONFIG_BBQUE_OPENCL)

# Linking PowerManagement support
if (CONFIG_BBQUE_PM)
target_link_libraries(barbeque
	bbque_pm
)
endif (CONFIG_BBQUE_PM)

# Linking EventManagement support
if (CONFIG_BBQUE_EM)
target_link_libraries(
	barbeque
	bbque_em
)
endif (CONFIG_BBQUE_EM)

# Linking network project
if (CONFIG_BBQUE_LINUX_CG_NET_BANDWIDTH)
#set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${PROJECT_BINARY_DIR}/lib")
target_link_libraries(barbeque
	netlink
)
endif (CONFIG_BBQUE_LINUX_CG_NET_BANDWIDTH)


# -------------- Static libraries and plugin linking  ------------------

# Recipe Loader
set (BBQUE_RLOADER  "bbque_rloader_${BBQUE_RLOADER_DEFAULT}")
target_link_libraries(barbeque
	-Wl,-whole-archive ${BBQUE_RLOADER} -Wl,-no-whole-archive
)
# Platform Loader
if (CONFIG_BBQUE_PIL_LOADER)
  set (BBQUE_PLOADER  "bbque_ploader_${BBQUE_PIL_LOADER_DEFAULT}")
  target_link_libraries(barbeque
	-Wl,-whole-archive ${BBQUE_PLOADER} -Wl,-no-whole-archive
  )
endif (CONFIG_BBQUE_PIL_LOADER)

# ----------------------------------------------------------------------

# Use link path ad RPATH
set_property(TARGET barbeque PROPERTY INSTALL_RPATH_USE_LINK_PATH TRUE)

# Install the configuration file
install(FILES "${PROJECT_BINARY_DIR}/include/bbque/config.h"
		DESTINATION "${BBQUE_PATH_HEADERS}")

# Install the Barbeque target
install (TARGETS barbeque RUNTIME
		DESTINATION ${BBQUE_PATH_BBQ}
		COMPONENT BarbequeRTRM)
# Install the Barbeque configuration files
install (FILES "${PROJECT_BINARY_DIR}/config/bbque.conf"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeRTRM)
install (FILES "${PROJECT_BINARY_DIR}/config/bbque.conf_dbg"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeRTRM)
install (FILES "${PROJECT_BINARY_DIR}/config/rtlib.conf"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeRTRM)
install (FILES "${PROJECT_BINARY_DIR}/config/bbque_2parma_demo2.conf"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeDEMOS)
# Install the daemon init script and its configuration file
install (PROGRAMS "${PROJECT_BINARY_DIR}/config/bbqued"
	DESTINATION ${BBQUE_PATH_INIT}
		COMPONENT BarbequeRTRM)
install (FILES "${PROJECT_BINARY_DIR}/config/default/bbqued"
	DESTINATION ${BBQUE_PATH_DEFAULTS}
		COMPONENT BarbequeRTRM)
# Install the BOSP Shell environment setup
install (FILES "${PROJECT_BINARY_DIR}/config/bosp_init.env"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeRTRM)
	# Install the HARPA-OS Shell environment setup
install (FILES "${PROJECT_BINARY_DIR}/config/harpa_init.env"
		DESTINATION ${BBQUE_PATH_CONF}
		COMPONENT BarbequeRTRM)
# Install the PIL configurations
install (PROGRAMS "${PROJECT_BINARY_DIR}/config/pil/bbque-pil-generator"
		DESTINATION ${BBQUE_PATH_BBQ}
		COMPONENT BarbequeRTRM)

if (CONFIG_BBQUE_TEST_PLATFORM_DATA)
  set (CONFIG_TARGET_ID "tpd")
  message(INFO "Setting CONFIG_TARGET_ID=${CONFIG_TARGET_ID}")
endif (CONFIG_BBQUE_TEST_PLATFORM_DATA)


if (CONFIG_TARGET_ID AND
  EXISTS "${PROJECT_SOURCE_DIR}/config/pil/${CONFIG_TARGET_ID}")
	install (FILES
		${PROJECT_SOURCE_DIR}/config/pil/${CONFIG_TARGET_ID}/systems.xml
		${PROJECT_SOURCE_DIR}/config/pil/${CONFIG_TARGET_ID}/${CONFIG_TARGET_ID}.xml
		DESTINATION ${BBQUE_PATH_PILS}
		COMPONENT BarbequeRTRM)
endif(CONFIG_TARGET_ID AND
  EXISTS "${PROJECT_SOURCE_DIR}/config/pil/${CONFIG_TARGET_ID}")


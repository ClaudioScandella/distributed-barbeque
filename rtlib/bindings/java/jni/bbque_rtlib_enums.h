#include <map>
#include <bbque/rtlib.h>

#ifndef _Included_bbque_rtlib_enums
#define _Included_bbque_rtlib_enums

/*
 * Define contract between JNI and 'RTLibExitCode' Java class
 */
#undef bbque_rtlib_enumeration_Constants_RTLIB_OK
#define bbque_rtlib_enumeration_Constants_RTLIB_OK 0L
#undef bbque_rtlib_enumeration_Constants_RTLIB_ERROR
#define bbque_rtlib_enumeration_Constants_RTLIB_ERROR 1L
#undef bbque_rtlib_enumeration_Constants_RTLIB_VERSION_MISMATCH
#define bbque_rtlib_enumeration_Constants_RTLIB_VERSION_MISMATCH 2L
#undef bbque_rtlib_enumeration_Constants_RTLIB_NO_WORKING_MODE
#define bbque_rtlib_enumeration_Constants_RTLIB_NO_WORKING_MODE 3L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_SETUP_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_SETUP_FAILED 4L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED 5L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_WRITE_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_WRITE_FAILED 6L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_FAILED 7L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_TIMEOUT
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_TIMEOUT 8L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH 9L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_UNAVAILABLE
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_UNAVAILABLE 10L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TIMEOUT
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TIMEOUT 11L
#undef bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_UNREACHABLE
#define bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_UNREACHABLE 12L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_DUPLICATE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_DUPLICATE 13L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_REGISTERED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_REGISTERED 14L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_REGISTRATION_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_REGISTRATION_FAILED 15L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_MISSING_RECIPE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_MISSING_RECIPE 16L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_UNREGISTRATION_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_UNREGISTRATION_FAILED 17L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_STARTED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_STARTED 18L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_ENABLE_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_ENABLE_FAILED 19L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_ENABLED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_ENABLED 20L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_DISABLE_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_DISABLE_FAILED 21L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_FAILED 22L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_START
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_START 23L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_RECONF
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_RECONF 24L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGREC
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGREC 25L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGRATE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGRATE 26L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_BLOCKED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_BLOCKED 27L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNC_MODE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNC_MODE 28L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNCP_FAILED
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNCP_FAILED 29L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_WORKLOAD_NONE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_WORKLOAD_NONE 30L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXC_CGROUP_NONE
#define bbque_rtlib_enumeration_Constants_RTLIB_EXC_CGROUP_NONE 31L
#undef bbque_rtlib_enumeration_Constants_RTLIB_EXIT_CODE_COUNT
#define bbque_rtlib_enumeration_Constants_RTLIB_EXIT_CODE_COUNT 32L

/*
 * Define contract between JNI and 'RTLibResourceType' Java class
 */
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_SYSTEM
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_SYSTEM 0L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_CPU
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_CPU 1L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_NR
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_NR 2L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_ELEMENT
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_ELEMENT 3L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_MEMORY
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_MEMORY 4L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_GPU
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_GPU 5L
#undef bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_ACCELERATOR
#define bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_ACCELERATOR 6L

/*
 * Define contract between JNI and 'RTLibConstraintOperation' Java class
 */
#undef bbque_rtlib_enumeration_Constants_CONSTRAINT_REMOVE
#define bbque_rtlib_enumeration_Constants_CONSTRAINT_REMOVE 0L
#undef bbque_rtlib_enumeration_Constants_CONSTRAINT_ADD
#define bbque_rtlib_enumeration_Constants_CONSTRAINT_ADD 1L

/*
 * Define contract between JNI and 'RTLibConstraintType' Java class
 */
#undef bbque_rtlib_enumeration_Constants_LOWER_BOUND
#define bbque_rtlib_enumeration_Constants_LOWER_BOUND 0L
#undef bbque_rtlib_enumeration_Constants_UPPER_BOUND
#define bbque_rtlib_enumeration_Constants_UPPER_BOUND 1L
#undef bbque_rtlib_enumeration_Constants_EXACT_VALUE
#define bbque_rtlib_enumeration_Constants_EXACT_VALUE 2L

static std::map<RTLIB_ExitCode_t, int> generateExitCodeCPPJMap();
static std::map<int, RTLIB_ExitCode_t> generateExitCodeJCPPMap();
static std::map<int, RTLIB_ResourceType_t> generateResourceTypeJCPPMap();
static std::map<int, RTLIB_ConstraintOperation_t> generateConstraintOperationJCPPMap();
static std::map<int, RTLIB_ConstraintType_t> generateConstraintTypeJCPPMap();

static int getJavaExitCode(RTLIB_ExitCode_t code);
static RTLIB_ExitCode_t getNativeExitCode(int java_exit_code);
static RTLIB_ResourceType_t getNativeResourceType(int java_resource_type);
static RTLIB_ConstraintOperation_t getNativeConstraintOperation(int java_constraint_operation);
static RTLIB_ConstraintType_t getNativeConstraintType(int java_constraint_type);

static std::map<RTLIB_ExitCode_t, int> map_exit_codes_cppj = generateExitCodeCPPJMap();
static std::map<int, RTLIB_ExitCode_t> map_exit_codes_jcpp = generateExitCodeJCPPMap();
static std::map<int, RTLIB_ResourceType_t> map_resource_types_jcpp = generateResourceTypeJCPPMap();
static std::map<int, RTLIB_ConstraintOperation_t> map_constraint_operations_jcpp = generateConstraintOperationJCPPMap();
static std::map<int, RTLIB_ConstraintType_t> map_constraint_types_jcpp = generateConstraintTypeJCPPMap();

static std::map<RTLIB_ExitCode_t, int> generateExitCodeCPPJMap() {
	std::map<RTLIB_ExitCode_t, int> m;
	m[RTLIB_OK] = bbque_rtlib_enumeration_Constants_RTLIB_OK;
	m[RTLIB_ERROR] = bbque_rtlib_enumeration_Constants_RTLIB_ERROR;
	m[RTLIB_VERSION_MISMATCH] = bbque_rtlib_enumeration_Constants_RTLIB_VERSION_MISMATCH;
	m[RTLIB_NO_WORKING_MODE] = bbque_rtlib_enumeration_Constants_RTLIB_NO_WORKING_MODE;
	m[RTLIB_BBQUE_CHANNEL_SETUP_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
	m[RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED;
	m[RTLIB_BBQUE_CHANNEL_WRITE_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	m[RTLIB_BBQUE_CHANNEL_READ_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_FAILED;
	m[RTLIB_BBQUE_CHANNEL_READ_TIMEOUT] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_TIMEOUT;
	m[RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH;
	m[RTLIB_BBQUE_CHANNEL_UNAVAILABLE] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_UNAVAILABLE;
	m[RTLIB_BBQUE_CHANNEL_TIMEOUT] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TIMEOUT;
	m[RTLIB_BBQUE_UNREACHABLE] = bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_UNREACHABLE;
	m[RTLIB_EXC_DUPLICATE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_DUPLICATE;
	m[RTLIB_EXC_NOT_REGISTERED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_REGISTERED;
	m[RTLIB_EXC_REGISTRATION_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_REGISTRATION_FAILED;
	m[RTLIB_EXC_MISSING_RECIPE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_MISSING_RECIPE;
	m[RTLIB_EXC_UNREGISTRATION_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_UNREGISTRATION_FAILED;
	m[RTLIB_EXC_NOT_STARTED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_STARTED;
	m[RTLIB_EXC_ENABLE_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_ENABLE_FAILED;
	m[RTLIB_EXC_NOT_ENABLED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_ENABLED;
	m[RTLIB_EXC_DISABLE_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_DISABLE_FAILED;
	m[RTLIB_EXC_GWM_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_FAILED;
	m[RTLIB_EXC_GWM_START] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_START;
	m[RTLIB_EXC_GWM_RECONF] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_RECONF;
	m[RTLIB_EXC_GWM_MIGREC] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGREC;
	m[RTLIB_EXC_GWM_MIGRATE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGRATE;
	m[RTLIB_EXC_GWM_BLOCKED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_BLOCKED;
	m[RTLIB_EXC_SYNC_MODE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNC_MODE;
	m[RTLIB_EXC_SYNCP_FAILED] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNCP_FAILED;
	m[RTLIB_EXC_WORKLOAD_NONE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_WORKLOAD_NONE;
	m[RTLIB_EXC_CGROUP_NONE] = bbque_rtlib_enumeration_Constants_RTLIB_EXC_CGROUP_NONE;
	m[RTLIB_EXIT_CODE_COUNT] = bbque_rtlib_enumeration_Constants_RTLIB_EXIT_CODE_COUNT;
	return m;
}

static std::map<int, RTLIB_ExitCode_t> generateExitCodeJCPPMap() {
	std::map<int, RTLIB_ExitCode_t> m;
	m[bbque_rtlib_enumeration_Constants_RTLIB_OK] = RTLIB_OK;
	m[bbque_rtlib_enumeration_Constants_RTLIB_ERROR] = RTLIB_ERROR;
	m[bbque_rtlib_enumeration_Constants_RTLIB_VERSION_MISMATCH] = RTLIB_VERSION_MISMATCH;
	m[bbque_rtlib_enumeration_Constants_RTLIB_NO_WORKING_MODE] = RTLIB_NO_WORKING_MODE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_SETUP_FAILED] = RTLIB_BBQUE_CHANNEL_SETUP_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED] = RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_WRITE_FAILED] = RTLIB_BBQUE_CHANNEL_WRITE_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_FAILED] = RTLIB_BBQUE_CHANNEL_READ_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_READ_TIMEOUT] = RTLIB_BBQUE_CHANNEL_READ_TIMEOUT;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH] = RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_UNAVAILABLE] = RTLIB_BBQUE_CHANNEL_UNAVAILABLE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_CHANNEL_TIMEOUT] = RTLIB_BBQUE_CHANNEL_TIMEOUT;
	m[bbque_rtlib_enumeration_Constants_RTLIB_BBQUE_UNREACHABLE] = RTLIB_BBQUE_UNREACHABLE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_DUPLICATE] = RTLIB_EXC_DUPLICATE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_REGISTERED] = RTLIB_EXC_NOT_REGISTERED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_REGISTRATION_FAILED] = RTLIB_EXC_REGISTRATION_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_MISSING_RECIPE] = RTLIB_EXC_MISSING_RECIPE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_UNREGISTRATION_FAILED] = RTLIB_EXC_UNREGISTRATION_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_STARTED] = RTLIB_EXC_NOT_STARTED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_ENABLE_FAILED] = RTLIB_EXC_ENABLE_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_NOT_ENABLED] = RTLIB_EXC_NOT_ENABLED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_DISABLE_FAILED] = RTLIB_EXC_DISABLE_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_FAILED] = RTLIB_EXC_GWM_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_START] = RTLIB_EXC_GWM_START;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_RECONF] = RTLIB_EXC_GWM_RECONF;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGREC] = RTLIB_EXC_GWM_MIGREC;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_MIGRATE] = RTLIB_EXC_GWM_MIGRATE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_GWM_BLOCKED] = RTLIB_EXC_GWM_BLOCKED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNC_MODE] = RTLIB_EXC_SYNC_MODE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_SYNCP_FAILED] = RTLIB_EXC_SYNCP_FAILED;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_WORKLOAD_NONE] = RTLIB_EXC_WORKLOAD_NONE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXC_CGROUP_NONE] = RTLIB_EXC_CGROUP_NONE;
	m[bbque_rtlib_enumeration_Constants_RTLIB_EXIT_CODE_COUNT] = RTLIB_EXIT_CODE_COUNT;
	return m;
}

static std::map<int, RTLIB_ResourceType_t> generateResourceTypeJCPPMap() {
	std::map<int, RTLIB_ResourceType_t> m;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_SYSTEM] = SYSTEM;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_CPU] = CPU;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_NR] = PROC_NR;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_PROC_ELEMENT] = PROC_ELEMENT;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_MEMORY] = MEMORY;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_GPU] = GPU;
	m[bbque_rtlib_enumeration_Constants_RESOURCE_TYPE_ACCELERATOR] = ACCELERATOR;
	return m;
}

static std::map<int, RTLIB_ConstraintOperation_t> generateConstraintOperationJCPPMap() {
	std::map<int, RTLIB_ConstraintOperation_t> m;
	m[bbque_rtlib_enumeration_Constants_CONSTRAINT_REMOVE] = CONSTRAINT_REMOVE;
	m[bbque_rtlib_enumeration_Constants_CONSTRAINT_ADD] = CONSTRAINT_ADD;
	return m;
}

static std::map<int, RTLIB_ConstraintType_t> generateConstraintTypeJCPPMap() {
	std::map<int, RTLIB_ConstraintType_t> m;
	m[bbque_rtlib_enumeration_Constants_LOWER_BOUND] = LOWER_BOUND;
	m[bbque_rtlib_enumeration_Constants_UPPER_BOUND] = UPPER_BOUND;
	m[bbque_rtlib_enumeration_Constants_EXACT_VALUE] = EXACT_VALUE;
	return m;
}

static int getJavaExitCode(RTLIB_ExitCode_t code) {
	std::map<RTLIB_ExitCode_t, int>::iterator i = map_exit_codes_cppj.find(code);
	if (i != map_exit_codes_cppj.end()) {
		return i->second;
	} else {
		return bbque_rtlib_enumeration_Constants_RTLIB_ERROR;
	}
}

static RTLIB_ExitCode_t getNativeExitCode(int java_exit_code) {
	std::map<int, RTLIB_ExitCode_t>::iterator i = map_exit_codes_jcpp.find(java_exit_code);
	if (i != map_exit_codes_jcpp.end()) {
		return i->second;
	} else {
		return RTLIB_ERROR;
	}
}

static RTLIB_ResourceType_t getNativeResourceType(int java_resource_type) {
	std::map<int, RTLIB_ResourceType_t>::iterator i = map_resource_types_jcpp.find(java_resource_type);
	if (i != map_resource_types_jcpp.end()) {
		return i->second;
	} else {
		return SYSTEM;
	}
}

static RTLIB_ConstraintOperation_t getNativeConstraintOperation(int java_constraint_operation) {
	std::map<int, RTLIB_ConstraintOperation_t>::iterator i = map_constraint_operations_jcpp.find(java_constraint_operation);
	if (i != map_constraint_operations_jcpp.end()) {
		return i->second;
	} else {
		return CONSTRAINT_REMOVE;
	}
}

static RTLIB_ConstraintType_t getNativeConstraintType(int java_constraint_type) {
	std::map<int, RTLIB_ConstraintType_t>::iterator i = map_constraint_types_jcpp.find(java_constraint_type);
	if (i != map_constraint_types_jcpp.end()) {
		return i->second;
	} else {
		return LOWER_BOUND;
	}
}

#endif

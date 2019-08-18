package bbque.rtlib.enumeration;

/*package-local*/ class Constants {

    /*package-local*/ static final int RTLIB_OK = 0;
    /*package-local*/ static final int RTLIB_ERROR = 1;
    /*package-local*/ static final int RTLIB_VERSION_MISMATCH = 2;
    /*package-local*/ static final int RTLIB_NO_WORKING_MODE = 3;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_SETUP_FAILED = 4;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED = 5;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_WRITE_FAILED = 6;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_READ_FAILED = 7;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_READ_TIMEOUT = 8;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH = 9;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_UNAVAILABLE = 10;
    /*package-local*/ static final int RTLIB_BBQUE_CHANNEL_TIMEOUT = 11;
    /*package-local*/ static final int RTLIB_BBQUE_UNREACHABLE = 12;
    /*package-local*/ static final int RTLIB_EXC_DUPLICATE = 13;
    /*package-local*/ static final int RTLIB_EXC_NOT_REGISTERED = 14;
    /*package-local*/ static final int RTLIB_EXC_REGISTRATION_FAILED = 15;
    /*package-local*/ static final int RTLIB_EXC_MISSING_RECIPE = 16;
    /*package-local*/ static final int RTLIB_EXC_UNREGISTRATION_FAILED = 17;
    /*package-local*/ static final int RTLIB_EXC_NOT_STARTED = 18;
    /*package-local*/ static final int RTLIB_EXC_ENABLE_FAILED = 19;
    /*package-local*/ static final int RTLIB_EXC_NOT_ENABLED = 20;
    /*package-local*/ static final int RTLIB_EXC_DISABLE_FAILED = 21;
    /*package-local*/ static final int RTLIB_EXC_GWM_FAILED = 22;
    /*package-local*/ static final int RTLIB_EXC_GWM_START = 23;
    /*package-local*/ static final int RTLIB_EXC_GWM_RECONF = 24;
    /*package-local*/ static final int RTLIB_EXC_GWM_MIGREC = 25;
    /*package-local*/ static final int RTLIB_EXC_GWM_MIGRATE = 26;
    /*package-local*/ static final int RTLIB_EXC_GWM_BLOCKED = 27;
    /*package-local*/ static final int RTLIB_EXC_SYNC_MODE = 28;
    /*package-local*/ static final int RTLIB_EXC_SYNCP_FAILED = 29;
    /*package-local*/ static final int RTLIB_EXC_WORKLOAD_NONE = 30;
    /*package-local*/ static final int RTLIB_EXC_CGROUP_NONE = 31;
    /*package-local*/ static final int RTLIB_EXIT_CODE_COUNT = 32;

    /*package-local*/ static final int RESOURCE_TYPE_SYSTEM = 0;
    /*package-local*/ static final int RESOURCE_TYPE_CPU = 1;
    /*package-local*/ static final int RESOURCE_TYPE_PROC_NR = 2;
    /*package-local*/ static final int RESOURCE_TYPE_PROC_ELEMENT = 3;
    /*package-local*/ static final int RESOURCE_TYPE_MEMORY = 4;
    /*package-local*/ static final int RESOURCE_TYPE_GPU = 5;
    /*package-local*/ static final int RESOURCE_TYPE_ACCELERATOR = 6;

    /*package-local*/ static final int CONSTRAINT_REMOVE = 0;
    /*package-local*/ static final int CONSTRAINT_ADD = 1;

    /*package-local*/ static final int LOWER_BOUND = 0;
    /*package-local*/ static final int UPPER_BOUND = 1;
    /*package-local*/ static final int EXACT_VALUE = 2;

}
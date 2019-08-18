package bbque.rtlib.enumeration;

import java.util.HashMap;
import java.util.Map;

public enum RTLibExitCode {
    /** Success (no errors) */
    RTLIB_OK(Constants.RTLIB_OK),
    /** Unspecified (generic) error */
    RTLIB_ERROR(Constants.RTLIB_ERROR),
    /** RTLibServices Version does not match that of the Barbeque RTRM */
    RTLIB_VERSION_MISMATCH(Constants.RTLIB_VERSION_MISMATCH),
    /** No new working mode error */
    RTLIB_NO_WORKING_MODE(Constants.RTLIB_NO_WORKING_MODE),
    /** Failed to setup the channel to connect the Barbeque RTRM */
    RTLIB_BBQUE_CHANNEL_SETUP_FAILED(Constants.RTLIB_BBQUE_CHANNEL_SETUP_FAILED),
    /** Failed to release the channel to connect the Barbeque RTRM */
    RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED(Constants.RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED),
    /** Failed to write to the Barbeque RTRM communication channel */
    RTLIB_BBQUE_CHANNEL_WRITE_FAILED(Constants.RTLIB_BBQUE_CHANNEL_WRITE_FAILED),
    /** Failed to read form the Barbeque RTRM communication channel */
    RTLIB_BBQUE_CHANNEL_READ_FAILED(Constants.RTLIB_BBQUE_CHANNEL_READ_FAILED),
    /** Timeout on read form the Barbeque RTRM communication channel */
    RTLIB_BBQUE_CHANNEL_READ_TIMEOUT(Constants.RTLIB_BBQUE_CHANNEL_READ_TIMEOUT),
    /** The bbque and application RPC protocol versions does not match */
    RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH(Constants.RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH),
    /** The (expected) communicaiton channel is not available */
    RTLIB_BBQUE_CHANNEL_UNAVAILABLE(Constants.RTLIB_BBQUE_CHANNEL_UNAVAILABLE),
    /** The (expected) response has gone in TIMEOUT */
    RTLIB_BBQUE_CHANNEL_TIMEOUT(Constants.RTLIB_BBQUE_CHANNEL_TIMEOUT),
    /** The Barbeque RTRM is not available */
    RTLIB_BBQUE_UNREACHABLE(Constants.RTLIB_BBQUE_UNREACHABLE),
    /** The Execution Context Duplicated */
    RTLIB_EXC_DUPLICATE(Constants.RTLIB_EXC_DUPLICATE),
    /** The Execution Context has not been registered */
    RTLIB_EXC_NOT_REGISTERED(Constants.RTLIB_EXC_NOT_REGISTERED),
    /** The Execution Context Registration Failed */
    RTLIB_EXC_REGISTRATION_FAILED(Constants.RTLIB_EXC_REGISTRATION_FAILED),
    /** The Execution Context Registration Failed */
    RTLIB_EXC_MISSING_RECIPE(Constants.RTLIB_EXC_MISSING_RECIPE),
    /** The Execution Context Unregistration Failed */
    RTLIB_EXC_UNREGISTRATION_FAILED(Constants.RTLIB_EXC_UNREGISTRATION_FAILED),
    /** The Execution Context has not been started yet */
    RTLIB_EXC_NOT_STARTED(Constants.RTLIB_EXC_NOT_STARTED),
    /** The Execution Context Start Failed */
    RTLIB_EXC_ENABLE_FAILED(Constants.RTLIB_EXC_ENABLE_FAILED),
    /** The Execution Context is not currently enabled */
    RTLIB_EXC_NOT_ENABLED(Constants.RTLIB_EXC_NOT_ENABLED),
    /** The Execution Context Stop Failed */
    RTLIB_EXC_DISABLE_FAILED(Constants.RTLIB_EXC_DISABLE_FAILED),
    /** Failed to get a working mode */
    RTLIB_EXC_GWM_FAILED(Constants.RTLIB_EXC_GWM_FAILED),
    /** Start running on the assigned AWM */
    RTLIB_EXC_GWM_START(Constants.RTLIB_EXC_GWM_START),
    /** Reconfiguration into a different AWM */
    RTLIB_EXC_GWM_RECONF(Constants.RTLIB_EXC_GWM_RECONF),
    /** Migration and reconfiguration into a different AWM */
    RTLIB_EXC_GWM_MIGREC(Constants.RTLIB_EXC_GWM_MIGREC),
    /** Migration (still running on the same AWM) */
    RTLIB_EXC_GWM_MIGRATE(Constants.RTLIB_EXC_GWM_MIGRATE),
    /** EXC suspended (resources not available) */
    RTLIB_EXC_GWM_BLOCKED(Constants.RTLIB_EXC_GWM_BLOCKED),
    /** The EXC is in sync mode */
    RTLIB_EXC_SYNC_MODE(Constants.RTLIB_EXC_SYNC_MODE),
    /** A step of the synchronization protocol has failed */
    RTLIB_EXC_SYNCP_FAILED(Constants.RTLIB_EXC_SYNCP_FAILED),
    /** No more workload to process */
    RTLIB_EXC_WORKLOAD_NONE(Constants.RTLIB_EXC_WORKLOAD_NONE),
    /** Unable to identify the CGRoup path */
    RTLIB_EXC_CGROUP_NONE(Constants.RTLIB_EXC_CGROUP_NONE),
    RTLIB_EXIT_CODE_COUNT(Constants.RTLIB_EXIT_CODE_COUNT);

    private static Map<Integer, RTLibExitCode> mJNIMap = generateJNIValueTranslation();

    private static Map<Integer, RTLibExitCode> generateJNIValueTranslation() {
        Map<Integer, RTLibExitCode> map = new HashMap<>();
        map.put(Constants.RTLIB_OK, RTLIB_OK);
        map.put(Constants.RTLIB_ERROR, RTLIB_ERROR);
        map.put(Constants.RTLIB_VERSION_MISMATCH, RTLIB_VERSION_MISMATCH);
        map.put(Constants.RTLIB_NO_WORKING_MODE, RTLIB_NO_WORKING_MODE);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_SETUP_FAILED, RTLIB_BBQUE_CHANNEL_SETUP_FAILED);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED, RTLIB_BBQUE_CHANNEL_TEARDOWN_FAILED);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_WRITE_FAILED, RTLIB_BBQUE_CHANNEL_WRITE_FAILED);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_READ_FAILED, RTLIB_BBQUE_CHANNEL_READ_FAILED);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_READ_TIMEOUT, RTLIB_BBQUE_CHANNEL_READ_TIMEOUT);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH, RTLIB_BBQUE_CHANNEL_PROTOCOL_MISMATCH);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_UNAVAILABLE, RTLIB_BBQUE_CHANNEL_UNAVAILABLE);
        map.put(Constants.RTLIB_BBQUE_CHANNEL_TIMEOUT, RTLIB_BBQUE_CHANNEL_TIMEOUT);
        map.put(Constants.RTLIB_BBQUE_UNREACHABLE, RTLIB_BBQUE_UNREACHABLE);
        map.put(Constants.RTLIB_EXC_DUPLICATE, RTLIB_EXC_DUPLICATE);
        map.put(Constants.RTLIB_EXC_NOT_REGISTERED, RTLIB_EXC_NOT_REGISTERED);
        map.put(Constants.RTLIB_EXC_REGISTRATION_FAILED, RTLIB_EXC_REGISTRATION_FAILED);
        map.put(Constants.RTLIB_EXC_MISSING_RECIPE, RTLIB_EXC_MISSING_RECIPE);
        map.put(Constants.RTLIB_EXC_UNREGISTRATION_FAILED, RTLIB_EXC_UNREGISTRATION_FAILED);
        map.put(Constants.RTLIB_EXC_NOT_STARTED, RTLIB_EXC_NOT_STARTED);
        map.put(Constants.RTLIB_EXC_ENABLE_FAILED, RTLIB_EXC_ENABLE_FAILED);
        map.put(Constants.RTLIB_EXC_NOT_ENABLED, RTLIB_EXC_NOT_ENABLED);
        map.put(Constants.RTLIB_EXC_DISABLE_FAILED, RTLIB_EXC_DISABLE_FAILED);
        map.put(Constants.RTLIB_EXC_GWM_FAILED, RTLIB_EXC_GWM_FAILED);
        map.put(Constants.RTLIB_EXC_GWM_START, RTLIB_EXC_GWM_START);
        map.put(Constants.RTLIB_EXC_GWM_RECONF, RTLIB_EXC_GWM_RECONF);
        map.put(Constants.RTLIB_EXC_GWM_MIGREC, RTLIB_EXC_GWM_MIGREC);
        map.put(Constants.RTLIB_EXC_GWM_MIGRATE, RTLIB_EXC_GWM_MIGRATE);
        map.put(Constants.RTLIB_EXC_GWM_BLOCKED, RTLIB_EXC_GWM_BLOCKED);
        map.put(Constants.RTLIB_EXC_SYNC_MODE, RTLIB_EXC_SYNC_MODE);
        map.put(Constants.RTLIB_EXC_SYNCP_FAILED, RTLIB_EXC_SYNCP_FAILED);
        map.put(Constants.RTLIB_EXC_WORKLOAD_NONE, RTLIB_EXC_WORKLOAD_NONE);
        map.put(Constants.RTLIB_EXC_CGROUP_NONE, RTLIB_EXC_CGROUP_NONE);
        map.put(Constants.RTLIB_EXIT_CODE_COUNT, RTLIB_EXIT_CODE_COUNT);
        return map;
    }

    private static RTLibExitCode fromJNIValue(int value) {
        RTLibExitCode exitCode = mJNIMap.get(value);
        if (exitCode != null) {
            return exitCode;
        }
        return RTLIB_ERROR;
    }

    private final int mJNIValue;

    RTLibExitCode(int value) {
        mJNIValue = value;
    }
}
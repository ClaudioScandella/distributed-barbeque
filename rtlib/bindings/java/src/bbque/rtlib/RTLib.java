package bbque.rtlib;

import bbque.rtlib.enumeration.RTLibExitCode;
import bbque.rtlib.exception.RTLibException;
import bbque.rtlib.model.RTLibServices;

public class RTLib {

    private static final String SHARED_LIBRARY_NAME = "bbque_java_rtlib";

    static {
        System.loadLibrary(SHARED_LIBRARY_NAME);
    }

    public static native RTLibServices init(String name) throws RTLibException;

    public static native String getErrorStr(RTLibExitCode exitCode);
}

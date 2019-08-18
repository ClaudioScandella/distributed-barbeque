package bbque.rtlib.exception;

import bbque.rtlib.enumeration.RTLibExitCode;

public class RTLibException extends Exception {

    private final RTLibExitCode mExitCode;

    public RTLibException(String message) {
        super(message);
        mExitCode = RTLibExitCode.RTLIB_ERROR;
    }

    public RTLibException(RTLibExitCode exitCode) {
        super("Exit code: " + exitCode.name());
        mExitCode = exitCode;
    }

    public RTLibExitCode getExitCode() {
        return mExitCode;
    }
}
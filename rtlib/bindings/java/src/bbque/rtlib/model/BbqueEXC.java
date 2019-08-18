package bbque.rtlib.model;

import bbque.rtlib.enumeration.RTLibExitCode;
import bbque.rtlib.enumeration.RTLibResourceType;
import bbque.rtlib.exception.*;

public abstract class BbqueEXC {

    private final long mNativePointer;

    public BbqueEXC(String name, String recipe, RTLibServices services) throws RTLibRegistrationException {
        mNativePointer = initNative(name, recipe, services);
    }

    private native long initNative(String name, String recipe, RTLibServices services);

    /***********************************************************************************************************
     ********************************** AEM EXECUTION CONTEXT MANAGEMENT ***************************************
     ***********************************************************************************************************/

    public native boolean isRegistered();

    public native void start() throws RTLibException;

    public native void waitCompletion() throws RTLibException;

    public native void terminate() throws RTLibException;

    public native void enable() throws RTLibException;

    public native void disable() throws RTLibException;

    /***********************************************************************************************************
     ************************************ AEM CONSTRAINTS MANAGEMENT *******************************************
     ***********************************************************************************************************/

    public native void setAWMConstraints(RTLibConstraint[] constraints) throws RTLibException;

    public native void clearAWMConstraints() throws RTLibException;

    public native void setGoalGap(int goalGapPercent) throws RTLibException;

    /***********************************************************************************************************
     ******************************************* AEM UTILITIES *************************************************
     ***********************************************************************************************************/

    public native String getUniqueID_String();

    public native int getUniqueID();

    public native int getAssignedResources(RTLibResourceType resourceType) throws RTLibException;

    public native int[] getAffinityMask(int[] ids) throws RTLibException;

    public native int[] getAssignedResources(RTLibResourceType resourceType, int[] systems) throws RTLibException;

    public native void setCPS(float cps) throws RTLibException;

    public native void setJPSGoal(float jpsMin, float jpsMax, int jpc) throws RTLibException;

    public native void updateJPC(int jpc) throws RTLibException;

    public native void setCPSGoal(float cpsMin, float cpsMax) throws RTLibException;

    public native void setCPSGoal(float cps) throws RTLibException;

    public native void setMinimumCycleTimeUs(int minCycleTimeUs) throws RTLibException;

    public native float getCPS();

    public native float getJPS();

    public native int getCTimeUs();

    public native int getCTimeMs();

    public native int cycles();

    public native RTLibWorkingModeParams workingModeParams();

    public native boolean done();

    public native int currentAWM();

    public native RTLibConfig configuration();

    /***********************************************************************************************************
     ************************************ AEM APPLICATION CALLBACKS ********************************************
     ***********************************************************************************************************/

    protected abstract void onSetup() throws RTLibException;

    protected abstract void onConfigure(int awmId) throws RTLibException;

    protected abstract void onSuspend() throws RTLibException;

    protected abstract void onResume() throws RTLibException;

    protected abstract void onRun() throws RTLibException;

    protected abstract void onMonitor() throws RTLibException;

    protected abstract void onRelease() throws RTLibException;

    /***********************************************************************************************************
     ************************************* NATIVE CALLBACKS BRIDGE *********************************************
     ***********************************************************************************************************/

    private RTLibExitCode onSetupCallback() {
        try {
            onSetup();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onConfigureCallback(int awmId) {
        try {
            onConfigure(awmId);
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onSuspendCallback() {
        try {
            onSuspend();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onResumeCallback() {
        try {
            onResume();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onRunCallback() {
        try {
            onRun();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onMonitorCallback() {
        try {
            onMonitor();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }

    private RTLibExitCode onReleaseCallback() {
        try {
            onRelease();
        } catch (RTLibException e) {
            return e.getExitCode();
        }
        return RTLibExitCode.RTLIB_OK;
    }
}
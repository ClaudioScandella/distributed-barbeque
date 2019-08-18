package bbque.rtlib.enumeration;

public enum RTLibResourceType {

    SYSTEM(Constants.RESOURCE_TYPE_SYSTEM),
    CPU(Constants.RESOURCE_TYPE_CPU),
    PROC_NR(Constants.RESOURCE_TYPE_PROC_NR),
    PROC_ELEMENT(Constants.RESOURCE_TYPE_PROC_ELEMENT),
    MEMORY(Constants.RESOURCE_TYPE_MEMORY),
    GPU(Constants.RESOURCE_TYPE_GPU),
    ACCELERATOR(Constants.RESOURCE_TYPE_ACCELERATOR);

    private final int mJNIValue;

    RTLibResourceType(int value) {
        mJNIValue = value;
    }
}
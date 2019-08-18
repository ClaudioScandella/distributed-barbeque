package bbque.rtlib.model;

public class RTLibServices {

    private final long mNativePointer;

    private RTLibServices(long nativePointer) {
        mNativePointer = nativePointer;
    }

    public native RTLibAPIVersion version();

    public native RTLibConfig config();
}
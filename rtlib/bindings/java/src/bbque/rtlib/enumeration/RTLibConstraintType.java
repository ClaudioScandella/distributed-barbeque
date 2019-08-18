package bbque.rtlib.enumeration;

public enum RTLibConstraintType {

    LOWER_BOUND(Constants.LOWER_BOUND),
    UPPER_BOUND(Constants.UPPER_BOUND),
    EXACT_VALUE(Constants.EXACT_VALUE);

    private final int mJNIValue;

    RTLibConstraintType(int value) {
        mJNIValue = value;
    }
}
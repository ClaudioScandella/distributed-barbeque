package bbque.rtlib.enumeration;

public enum RTLibConstraintOperation {

    CONSTRAINT_REMOVE(Constants.CONSTRAINT_REMOVE),
    CONSTRAINT_ADD(Constants.CONSTRAINT_ADD);

    private final int mJNIValue;

    RTLibConstraintOperation(int value) {
        mJNIValue = value;
    }
}

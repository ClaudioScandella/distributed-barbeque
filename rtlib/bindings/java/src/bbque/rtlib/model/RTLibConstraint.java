package bbque.rtlib.model;

import bbque.rtlib.enumeration.RTLibConstraintOperation;
import bbque.rtlib.enumeration.RTLibConstraintType;

public class RTLibConstraint {

    private final int mAwm;
    private final RTLibConstraintOperation mOperation;
    private final RTLibConstraintType mType;

    public RTLibConstraint(int awm, RTLibConstraintOperation operation, RTLibConstraintType type) {
        mAwm = awm;
        mOperation = operation;
        mType = type;
    }

    public int awm() {
        return mAwm;
    }

    public RTLibConstraintOperation operation() {
        return mOperation;
    }

    public RTLibConstraintType type() {
        return mType;
    }
}
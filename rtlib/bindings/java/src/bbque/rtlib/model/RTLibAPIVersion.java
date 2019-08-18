package bbque.rtlib.model;

public class RTLibAPIVersion {

    private final int mMajor;
    private final int mMinor;

    private RTLibAPIVersion(int major, int minor) {
        mMajor = major;
        mMinor = minor;
    }

    public int getMajor() {
        return mMajor;
    }

    public int getMinor() {
        return mMinor;
    }

    @Override
    public String toString() {
        return "{ major: " +
                mMajor +
                ", minor: " +
                mMinor +
                " }";
    }
}
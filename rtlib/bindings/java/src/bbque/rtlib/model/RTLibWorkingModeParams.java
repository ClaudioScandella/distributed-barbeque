package bbque.rtlib.model;

public class RTLibWorkingModeParams {

    private final int mAwmId;
    private final RTLibServices mServices;
    private final RTLibSystemResources[] mSystemResources;

    private RTLibWorkingModeParams(int awmId, RTLibServices services, RTLibSystemResources[] systemResources) {
        mAwmId = awmId;
        mServices = services;
        mSystemResources = systemResources;
    }

    public int awmId() {
        return mAwmId;
    }

    public RTLibServices services() {
        return mServices;
    }

    public int nrSys() {
        return mSystemResources.length;
    }

    public RTLibSystemResources[] systemResources() {
        return mSystemResources;
    }

    @Override
    public String toString() {
        StringBuilder builder = new StringBuilder()
                .append("{ awm_id: ")
                .append(mAwmId)
                .append(", nr_sys: ")
                .append(mSystemResources.length)
                .append(", system_resources: [");
        for (RTLibSystemResources mSystemResource : mSystemResources) {
            builder.append(mSystemResource.toString());
        }
        return builder.append("]}").toString();
    }
}
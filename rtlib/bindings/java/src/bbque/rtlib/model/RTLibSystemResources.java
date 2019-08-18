package bbque.rtlib.model;

public class RTLibSystemResources {

    private final int mSystemId;
    private final int mNumberOfCPUs;
    private final int mNumberOfProcElements;
    private final int mCpuBandwidth;
    private final int mMemBandwidth;

    private final int mGpuBandwidth;
    private final int mAcceleratorBandwidth;
    private final int mOclDeviceId;

    private RTLibSystemResources(int systemId, int numberOfCPUs, int numberOfProcElements,
                      int cpuBandwidth, int memBandwidth, int gpuBandwidth,
                      int acceleratorBandwidth, int oclDeviceId) {
        mSystemId = systemId;
        mNumberOfCPUs = numberOfCPUs;
        mNumberOfProcElements = numberOfProcElements;
        mCpuBandwidth = cpuBandwidth;
        mMemBandwidth = memBandwidth;
        mGpuBandwidth = gpuBandwidth;
        mAcceleratorBandwidth = acceleratorBandwidth;
        mOclDeviceId = oclDeviceId;
    }

    public int sysId() {
        return mSystemId;
    }

    public int numberCPUs() {
        return mNumberOfCPUs;
    }

    public int numberProcElements() {
        return mNumberOfProcElements;
    }

    public int cpuBandwidth() {
        return mCpuBandwidth;
    }

    public int memBandwidth() {
        return mMemBandwidth;
    }

    public int gpuBandwidth() {
        return mGpuBandwidth;
    }

    public int acceleratorBandwidth() {
        return mAcceleratorBandwidth;
    }

    public int oclDeviceId() {
        return mOclDeviceId;
    }

    @Override
    public String toString() {
        return "{ sys_id: " +
                mSystemId +
                ", number_cpus: " +
                mNumberOfCPUs +
                ", number_proc_elements: " +
                mNumberOfProcElements +
                ", cpu_bandwidth: " +
                mCpuBandwidth +
                ", mem_bandwidth: " +
                mMemBandwidth +
                ", gpu_bandwidth: " +
                mGpuBandwidth +
                ", accelerator_bandwidth: " +
                mAcceleratorBandwidth +
                ", ocl_device_id: " +
                mOclDeviceId +
                "}";
    }
}
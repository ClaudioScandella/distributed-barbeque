package bbque.rtlib.model;

public class RTLibConfig {

    private final Profiling mProfiling;
    private final RuntimeProfiling mRuntimeProfiling;
    private final Unmanaged mUnmanaged;
    private final CGroupSupport mCGroupSupport;
    private final Duration mDuration;

    private RTLibConfig(Profiling profiling, RuntimeProfiling runtimeProfiling,
                        Unmanaged unmanaged, CGroupSupport cGroupSupport, Duration duration) {
        mProfiling = profiling;
        mRuntimeProfiling = runtimeProfiling;
        mUnmanaged = unmanaged;
        mCGroupSupport = cGroupSupport;
        mDuration = duration;
    }

    public Profiling profiling() {
        return mProfiling;
    }

    public RuntimeProfiling runtimeProfiling() {
        return mRuntimeProfiling;
    }

    public Unmanaged unmanaged() {
        return mUnmanaged;
    }

    public CGroupSupport cGroupSupport() {
        return mCGroupSupport;
    }

    public Duration duration() {
        return mDuration;
    }

    public static class Profiling {

        private final boolean mEnabled;
        private final PerfCounters mPerfCounters;
        private final Output mOutput;
        private final OpenCL mOpenCL;

        private Profiling(boolean enabled, PerfCounters perfCounters, Output output, OpenCL openCL) {
            mEnabled = enabled;
            mPerfCounters = perfCounters;
            mOutput = output;
            mOpenCL = openCL;
        }

        public boolean enabled() {
            return mEnabled;
        }

        public PerfCounters perfCounter() {
            return mPerfCounters;
        }

        public Output output() {
            return mOutput;
        }

        public OpenCL opencl() {
            return mOpenCL;
        }

        public static class PerfCounters {

            private final boolean mGlobal;
            private final boolean mOverheads;
            private final boolean mNoKernel;
            private final boolean mBigNum;
            private final int mDetailedRun;
            private final int mRaw;

            private PerfCounters(boolean global, boolean overheads, boolean noKernel,
                                boolean bigNum, int detailedRun, int raw) {
                mGlobal = global;
                mOverheads = overheads;
                mNoKernel = noKernel;
                mBigNum = bigNum;
                mDetailedRun = detailedRun;
                mRaw = raw;
            }

            public boolean global() {
                return mGlobal;
            }

            public boolean overheads() {
                return mOverheads;
            }

            public boolean noKernel() {
                return mNoKernel;
            }

            public boolean bigNum() {
                return mBigNum;
            }

            public int detailedRun() {
                return mDetailedRun;
            }

            public int raw() {
                return mRaw;
            }
        }

        public static class Output {

            private final boolean mFile;
            private final CSV mCSV;

            private Output(boolean file, CSV csv) {
                mFile = file;
                mCSV = csv;
            }

            public boolean file() {
                return mFile;
            }

            public CSV CSV() {
                return mCSV;
            }

            public static class CSV {

                private final boolean mEnabled;
                private final String mSeparator;

                private CSV(boolean enabled, String separator) {
                    mEnabled = enabled;
                    mSeparator = separator;
                }

                public boolean enabled() {
                    return mEnabled;
                }

                public String separator() {
                    return mSeparator;
                }
            }

        }

        public static class OpenCL {

            private final boolean mEnabled;
            private final int mLevel;

            private OpenCL(boolean enabled, int level) {
                mEnabled = enabled;
                mLevel = level;
            }

            public boolean enabled() {
                return mEnabled;
            }

            public int level() {
                return mLevel;
            }
        }
    }

    public static class RuntimeProfiling {

        private final int mRtProfileRearmTimeMs;
        private final int mRtProfileWaitForSyncMs;

        private RuntimeProfiling(int rtProfileRearmTimeMs, int rtProfileWaitForSyncMs) {
            mRtProfileRearmTimeMs = rtProfileRearmTimeMs;
            mRtProfileWaitForSyncMs = rtProfileWaitForSyncMs;
        }

        public int rtProfileRearmTimeMs() {
            return mRtProfileRearmTimeMs;
        }

        public int rtProfileWaitForSyncMs() {
            return mRtProfileWaitForSyncMs;
        }
    }

    public static class Unmanaged {

        private final boolean mEnabled;
        private final int mAwmId;

        private Unmanaged(boolean enabled, int awmId) {
            mEnabled = enabled;
            mAwmId = awmId;
        }

        public boolean enabled() {
            return mEnabled;
        }

        public int awmId() {
            return mAwmId;
        }

    }

    public static class CGroupSupport {

        private final boolean mEnabled;
        private final boolean mStaticConfiguration;
        private final CpuSet mCpuSet;
        private final Cpu mCpu;
        private final Memory mMemory;

        private CGroupSupport(boolean enabled, boolean staticConfiguration,
                              CpuSet cpuSet, Cpu cpu, Memory memory) {
            mEnabled = enabled;
            mStaticConfiguration = staticConfiguration;
            mCpuSet = cpuSet;
            mCpu = cpu;
            mMemory = memory;
        }

        public boolean enabled() {
            return mEnabled;
        }

        public boolean staticConfiguration() {
            return mStaticConfiguration;
        }

        public CpuSet cpuSet() {
            return mCpuSet;
        }

        public Cpu cpu() {
            return mCpu;
        }

        public Memory memory() {
            return mMemory;
        }

        public static class CpuSet {

            private final String mCpus;
            private final String mMems;

            private CpuSet(String cpus, String mems) {
                mCpus = cpus;
                mMems = mems;
            }

            public String cpus() {
                return mCpus;
            }

            public String mems() {
                return mMems;
            }
        }

        public static class Cpu {

            private final String mCfsPeriodUs;
            private final String mCfsQuotaUs;

            private Cpu(String cfsPeriodUs, String cfsQuotaUs) {
                mCfsPeriodUs = cfsPeriodUs;
                mCfsQuotaUs = cfsQuotaUs;
            }

            public String cfsPeriodUs() {
                return mCfsPeriodUs;
            }

            public String cfsQuotaUs() {
                return mCfsQuotaUs;
            }
        }

        public static class Memory {

            private final String mLimitInBytes;

            private Memory(String limitInBytes) {
                mLimitInBytes = limitInBytes;
            }

            public String limitInBytes() {
                return mLimitInBytes;
            }
        }
    }

    public static class Duration {

        private final boolean mEnabled;
        private final boolean mTimeLimit;
        private final int mMaxCyclesBeforeTermination;
        private final int mMaxMsBeforeTermination;

        private Duration(boolean enabled, boolean timeLimit, int maxCyclesBeforeTermination,
                         int maxMsBeforeTermination) {
            mEnabled = enabled;
            mTimeLimit = timeLimit;
            mMaxCyclesBeforeTermination = maxCyclesBeforeTermination;
            mMaxMsBeforeTermination = maxMsBeforeTermination;
        }

        public boolean enabled() {
            return mEnabled;
        }

        public boolean timeLimit() {
            return mTimeLimit;
        }

        public int maxCyclesBeforeTermination() {
            return mMaxCyclesBeforeTermination;
        }

        public int maxMsBeforeTermination() {
            return mMaxMsBeforeTermination;
        }
    }

    @Override
    public String toString() {
        return "{ profile: " +
                "{ enabled: " +
                mProfiling.mEnabled +
                ", perf_counters: " +
                "{ global: " +
                mProfiling.mPerfCounters.mGlobal +
                ", overheads: " +
                mProfiling.mPerfCounters.mOverheads +
                ", no_kernel: " +
                mProfiling.mPerfCounters.mNoKernel +
                ", big_num: " +
                mProfiling.mPerfCounters.mBigNum +
                ", detailed_run: " +
                mProfiling.mPerfCounters.mDetailedRun +
                ", raw: " +
                mProfiling.mPerfCounters.mRaw +
                "}, output: " +
                "{ file: " +
                mProfiling.mOutput.mFile +
                ", CSV: " +
                "{ enabled: " +
                mProfiling.mOutput.mCSV.mEnabled +
                ", separator: '" +
                mProfiling.mOutput.mCSV.mSeparator +
                "'}}, opencl: " +
                "{ enabled: " +
                mProfiling.mOpenCL.mEnabled +
                ", level: " +
                mProfiling.mOpenCL.mLevel +
                "}}, runtime_profiling: " +
                "{ rt_profile_rearm_time_ms: " +
                mRuntimeProfiling.mRtProfileRearmTimeMs +
                ", rt_profile_wait_for_sync_ms: " +
                mRuntimeProfiling.mRtProfileWaitForSyncMs +
                "}, unmanaged: " +
                "{ enabled: " +
                mUnmanaged.mEnabled +
                ", awm_id: " +
                mUnmanaged.mAwmId +
                "}, cgroup_support: " +
                "{ enabled: " +
                mCGroupSupport.mEnabled +
                ", static_configuration: " +
                mCGroupSupport.mStaticConfiguration +
                ", cpuset: " +
                "{ cpus: " +
                (mCGroupSupport.mCpuSet.mCpus == null ? "null" : "'" + mCGroupSupport.mCpuSet.mCpus + "'") +
                ", mems: " +
                (mCGroupSupport.mCpuSet.mMems == null ? "null" : "'" + mCGroupSupport.mCpuSet.mMems + "'") +
                "}, cpu: " +
                "{ cfs_period_us: " +
                (mCGroupSupport.mCpu.mCfsPeriodUs == null ? "null" : "'" + mCGroupSupport.mCpu.mCfsPeriodUs + "'") +
                ", cfs_quota_us: " +
                (mCGroupSupport.mCpu.mCfsQuotaUs == null ? "null" : "'" + mCGroupSupport.mCpu.mCfsQuotaUs + "'") +
                "}, memory: " +
                "{ limit_in_bytes: " +
                (mCGroupSupport.mMemory.mLimitInBytes == null ? "null" : "'" + mCGroupSupport.mMemory.mLimitInBytes + "'") +
                "}}, duration: " +
                "{ enabled: " +
                mDuration.mEnabled +
                ", time_limit: " +
                mDuration.mTimeLimit +
                ", max_cycles_before_termination: " +
                mDuration.mMaxCyclesBeforeTermination +
                ", max_ms_before_termination: " +
                mDuration.mMaxMsBeforeTermination +
                "}}";
    }
}
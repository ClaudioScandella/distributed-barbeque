#include <jni.h>
#include <bbque/rtlib.h>
#include "bbque_rtlib_commons.h"
#include "bbque_rtlib_enums.h"

/*
 * This function looks for the internal long field of the provided java object and returns it
 * as a jlong object that can be casted to the pointer to the native object.
 */
jlong getObjectNativePointer(JNIEnv *env, jobject obj) {
	jclass java_class = env->GetObjectClass(obj);
	jfieldID native_pointer_id = env->GetFieldID(java_class, "mNativePointer", "J");
	return env->GetLongField(obj, native_pointer_id);
}

/*
 * This function is used to instantiate a new java RTLibServices class from a valid native pointer.
 */
jobject createRTLibServicesObjFromPointer(JNIEnv *env, jlong native_pointer) {
	jclass java_services_class = env->FindClass("bbque/rtlib/model/RTLibServices");
	jmethodID java_services_method_id = env->GetMethodID(java_services_class, "<init>", "(J)V");
	return env->NewObject(java_services_class, java_services_method_id, native_pointer);
}

/*
 * This function is used to instantiate a new java RTLibConfig class from a valid native object.
 */
jobject createRTLibConfigObjFromNativeObj(JNIEnv *env, RTLIB_Conf_t config) {
	jclass java_profiling_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Profiling");
	jmethodID java_profiling_method_id = env->GetMethodID(java_profiling_class, "<init>", "(Z"
						"Lbbque/rtlib/model/RTLibConfig$Profiling$PerfCounters;"
						"Lbbque/rtlib/model/RTLibConfig$Profiling$Output;"
						"Lbbque/rtlib/model/RTLibConfig$Profiling$OpenCL;)V");
	jclass java_perf_counters_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Profiling$PerfCounters");
	jmethodID java_perf_counters_method_id = env->GetMethodID(java_perf_counters_class, "<init>", "(ZZZZII)V");
	jobject java_perf_counters = env->NewObject(java_perf_counters_class, java_perf_counters_method_id, 
						config.profile.perf_counters.global, config.profile.perf_counters.overheads,
						config.profile.perf_counters.no_kernel, config.profile.perf_counters.big_num,
						config.profile.perf_counters.detailed_run, config.profile.perf_counters.raw);
	jclass java_output_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Profiling$Output");
	jmethodID java_output_method_id = env->GetMethodID(java_output_class, "<init>", "(ZLbbque/rtlib/model/RTLibConfig$Profiling$Output$CSV;)V");
	jclass java_csv_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Profiling$Output$CSV");
	jmethodID java_csv_method_id = env->GetMethodID(java_csv_class, "<init>", "(ZLjava/lang/String;)V");
	jstring java_separator = env->NewStringUTF(config.profile.output.CSV.separator);
	jobject java_csv = env->NewObject(java_csv_class, java_csv_method_id, config.profile.output.CSV.enabled, java_separator);
	jobject java_output = env->NewObject(java_output_class, java_output_method_id, config.profile.output.file, java_csv);
	jclass java_opencl_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Profiling$OpenCL");
	jmethodID java_opencl_method_id = env->GetMethodID(java_opencl_class, "<init>", "(ZI)V");
	jobject java_opencl = env->NewObject(java_opencl_class, java_opencl_method_id, config.profile.opencl.enabled, config.profile.opencl.level);
	jobject java_profiling = env->NewObject(java_profiling_class, java_profiling_method_id, config.profile.enabled, java_perf_counters,
						java_output, java_opencl);
	jclass java_runtime_profiling_class = env->FindClass("bbque/rtlib/model/RTLibConfig$RuntimeProfiling");
	jmethodID java_runtime_profiling_method_id = env->GetMethodID(java_runtime_profiling_class, "<init>", "(II)V");
	jobject java_runtime_profiling = env->NewObject(java_runtime_profiling_class, java_runtime_profiling_method_id,
		config.runtime_profiling.rt_profile_rearm_time_ms, config.runtime_profiling.rt_profile_wait_for_sync_ms);
	jclass java_unmanaged_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Unmanaged");
	jmethodID java_unmanaged_method_id = env->GetMethodID(java_unmanaged_class, "<init>", "(ZI)V");
	jobject java_unmanaged = env->NewObject(java_unmanaged_class, java_unmanaged_method_id, config.unmanaged.enabled, config.unmanaged.awm_id);
	jclass java_cgroup_support_class = env->FindClass("bbque/rtlib/model/RTLibConfig$CGroupSupport");
	jmethodID java_cgroup_support_method_id = env->GetMethodID(java_cgroup_support_class, "<init>","(ZZ"
						"Lbbque/rtlib/model/RTLibConfig$CGroupSupport$CpuSet;"
						"Lbbque/rtlib/model/RTLibConfig$CGroupSupport$Cpu;"
						"Lbbque/rtlib/model/RTLibConfig$CGroupSupport$Memory;)V");
	jclass java_cpuset_class = env->FindClass("bbque/rtlib/model/RTLibConfig$CGroupSupport$CpuSet");
	jmethodID java_cpuset_method_id = env->GetMethodID(java_cpuset_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
	jstring java_cpus = env->NewStringUTF(config.cgroup_support.cpuset.cpus);
	jstring java_mems = env->NewStringUTF(config.cgroup_support.cpuset.mems);
	jobject java_cpuset = env->NewObject(java_cpuset_class, java_cpuset_method_id, java_cpus, java_mems);
	jclass java_cpu_class = env->FindClass("bbque/rtlib/model/RTLibConfig$CGroupSupport$Cpu");
	jmethodID java_cpu_method_id = env->GetMethodID(java_cpu_class, "<init>", "(Ljava/lang/String;Ljava/lang/String;)V");
	jstring java_cfs_period_us = env->NewStringUTF(config.cgroup_support.cpu.cfs_period_us);
	jstring java_cfs_quota_us = env->NewStringUTF(config.cgroup_support.cpu.cfs_quota_us);
	jobject java_cpu = env->NewObject(java_cpu_class, java_cpu_method_id, java_cfs_period_us, java_cfs_quota_us);
	jclass java_memory_class = env->FindClass("bbque/rtlib/model/RTLibConfig$CGroupSupport$Memory");
	jmethodID java_memory_method_id = env->GetMethodID(java_memory_class, "<init>", "(Ljava/lang/String;)V");
	jstring java_limit_in_bytes = env->NewStringUTF(config.cgroup_support.memory.limit_in_bytes);
	jobject java_memory = env->NewObject(java_memory_class, java_memory_method_id, java_limit_in_bytes);
	jobject java_cgroup_support = env->NewObject(java_cgroup_support_class, java_cgroup_support_method_id, config.cgroup_support.enabled,
						config.cgroup_support.static_configuration, java_cpuset, java_cpu, java_memory);
	jclass java_duration_class = env->FindClass("bbque/rtlib/model/RTLibConfig$Duration");
	jmethodID java_duration_method_id = env->GetMethodID(java_duration_class, "<init>", "(ZZII)V");
	jobject java_duration = env->NewObject(java_duration_class, java_duration_method_id, config.duration.enabled, config.duration.time_limit,
						config.duration.max_cycles_before_termination, config.duration.max_ms_before_termination);
	jclass java_config_class = env->FindClass("bbque/rtlib/model/RTLibConfig");
	jmethodID java_config_method_id = env->GetMethodID(java_config_class, "<init>", "(Lbbque/rtlib/model/RTLibConfig$Profiling;"
											"Lbbque/rtlib/model/RTLibConfig$RuntimeProfiling;"
											"Lbbque/rtlib/model/RTLibConfig$Unmanaged;"
											"Lbbque/rtlib/model/RTLibConfig$CGroupSupport;"
											"Lbbque/rtlib/model/RTLibConfig$Duration;)V");
	return env->NewObject(java_config_class, java_config_method_id, java_profiling, java_runtime_profiling,
				java_unmanaged, java_cgroup_support, java_duration);
}

/*
 * This function checks if the native exit code is an error or not.
 * In case of error it throws a java exception passing the corresponding java exit code.
 */
void throwRTLibExceptionIfNecessary(JNIEnv *env, RTLIB_ExitCode_t exit_code) {
	if (exit_code != RTLIB_OK) {
		int java_code = getJavaExitCode(exit_code);
		jclass java_exit_code_class = env->FindClass("bbque/rtlib/enumeration/RTLibExitCode");
		jmethodID java_exit_code_met = env->GetStaticMethodID(java_exit_code_class, "fromJNIValue", "(I)Lbbque/rtlib/enumeration/RTLibExitCode;");
		jobject java_exit_code = env->CallStaticObjectMethod(java_exit_code_class, java_exit_code_met, java_code);
		jclass java_exception_class = env->FindClass("bbque/rtlib/exception/RTLibException");
		jmethodID java_exception_method_id = env->GetMethodID(java_exception_class, "<init>", "(Lbbque/rtlib/enumeration/RTLibExitCode;)V");
		jobject java_exception = env->NewObject(java_exception_class, java_exception_method_id, java_exit_code);
		env->Throw((jthrowable) java_exception);
	}
}

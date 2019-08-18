#include <jni.h>
#include <bbque/rtlib.h>
#include "bbque_rtlib.h"
#include "bbque_rtlib_commons.h"
#include "bbque_rtlib_enums.h"

jobject Java_bbque_rtlib_RTLib_init(JNIEnv *env, jclass java_class, jstring java_name) {
	const char *native_name = env->GetStringUTFChars(java_name, JNI_FALSE);
	RTLIB_Services_t *rtlib;
	RTLIB_ExitCode_t exit_code = RTLIB_Init(native_name, &rtlib);
	env->ReleaseStringUTFChars(java_name, native_name);
	jlong native_pointer = reinterpret_cast<jlong>(rtlib);
	throwRTLibExceptionIfNecessary(env, exit_code);
	return createRTLibServicesObjFromPointer(env, native_pointer);
}

jstring Java_bbque_rtlib_RTLib_getErrorStr(JNIEnv *env, jclass java_class, jobject java_exit_code) {
	jclass java_exit_code_class = env->GetObjectClass(java_exit_code);
	jfieldID java_exit_code_value_id = env->GetFieldID(java_exit_code_class, "mJNIValue", "I");
	jint jni_exit_code_value = env->GetIntField(java_exit_code, java_exit_code_value_id);
	RTLIB_ExitCode_t exit_code = getNativeExitCode(jni_exit_code_value);
	const char *error_str = RTLIB_ErrorStr(exit_code);
	return env->NewStringUTF(error_str);
}

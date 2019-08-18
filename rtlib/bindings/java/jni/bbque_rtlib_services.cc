#include <jni.h>
#include <bbque/rtlib.h>
#include "bbque_rtlib_services.h"
#include "bbque_rtlib_commons.h"

jobject Java_bbque_rtlib_model_RTLibServices_version(JNIEnv *env, jobject obj) {
	jlong native_pointer = getObjectNativePointer(env, obj);
	RTLIB_Services_t *jni_services = reinterpret_cast<RTLIB_Services_t *>(native_pointer);
	jclass java_api_version_class = env->FindClass("bbque/rtlib/model/RTLibAPIVersion");
	jmethodID java_api_version_method_id = env->GetMethodID(java_api_version_class, "<init>", "(II)V");
	return env->NewObject(java_api_version_class, java_api_version_method_id, jni_services->version.major, jni_services->version.minor);
}

jobject Java_bbque_rtlib_model_RTLibServices_config(JNIEnv *env, jobject obj) {
	jlong native_pointer = getObjectNativePointer(env, obj);
	RTLIB_Services_t *jni_services = reinterpret_cast<RTLIB_Services_t *>(native_pointer);
	const RTLIB_Conf_t *config = jni_services->config;
	return createRTLibConfigObjFromNativeObj(env, *config);
}

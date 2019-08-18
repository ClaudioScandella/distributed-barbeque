#include <jni.h>
#include <bbque/rtlib.h>
#ifndef _Included_bbque_rtlib_commons
#define _Included_bbque_rtlib_commons

/*
 * This function is used to extract the native pointer from the instance of a java object
 * and return it as a jlong object.
 */
jlong getObjectNativePointer(JNIEnv *, jobject);

/*
 * This function is used to instantiate a new java RTLibServices class from a valid native pointer.
 */
jobject createRTLibServicesObjFromPointer(JNIEnv *, jlong);

/*
 * This function is used to instantiate a new java RTLibConfig class from a valid native object.
 */
jobject createRTLibConfigObjFromNativeObj(JNIEnv *, RTLIB_Conf_t);

/*
 * This function checks if it is necessary to throw a java exception looking at the provided exit code.
 */
void throwRTLibExceptionIfNecessary(JNIEnv *, RTLIB_ExitCode_t);

#endif

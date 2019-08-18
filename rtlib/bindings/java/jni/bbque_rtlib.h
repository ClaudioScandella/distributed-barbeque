#include <jni.h>

#ifndef _Included_bbque_rtlib_RTLib
#define _Included_bbque_rtlib_RTLib

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     bbque_rtlib_RTLib
 * Method:    init
 * Signature: (Ljava/lang/String;)Lbbque/rtlib/model/RTLibServices;
 */
JNIEXPORT jobject JNICALL Java_bbque_rtlib_RTLib_init
  (JNIEnv *, jclass, jstring);

/*
 * Class:     bbque_rtlib_RTLib
 * Method:    getErrorStr
 * Signature: (Lbbque/rtlib/enumeration/RTLibExitCode;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bbque_rtlib_RTLib_getErrorStr
  (JNIEnv *, jclass, jobject);

#ifdef __cplusplus
}
#endif

#endif

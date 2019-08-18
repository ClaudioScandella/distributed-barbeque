#include <jni.h>

#ifndef _Included_bbque_rtlib_model_RTLibServices
#define _Included_bbque_rtlib_model_RTLibServices

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     bbque_rtlib_model_RTLibServices
 * Method:    version
 * Signature: ()Lbbque/rtlib/model/RTLibAPIVersion;
 */
JNIEXPORT jobject JNICALL Java_bbque_rtlib_model_RTLibServices_version
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_RTLibServices
 * Method:    config
 * Signature: ()Lbbque/rtlib/model/RTLibConfig;
 */
JNIEXPORT jobject JNICALL Java_bbque_rtlib_model_RTLibServices_config
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif

#endif

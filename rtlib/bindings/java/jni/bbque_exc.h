#include <jni.h>
#include <bbque/bbque_exc.h>

#ifndef _Included_bbque_rtlib_model_BbqueEXC
#define _Included_bbque_rtlib_model_BbqueEXC

using namespace bbque::rtlib;

class JNIBbqueEXC : public BbqueEXC {

public:

	JNIBbqueEXC(std::string const & name, std::string const & recipe, RTLIB_Services_t *rtlib, JNIEnv *env, jobject obj)
												: BbqueEXC(name, recipe, rtlib) {
		env->GetJavaVM(&jvm);
		callback_obj = env->NewGlobalRef(obj);
		jclass obj_class = env->GetObjectClass(obj);
		callback_class = reinterpret_cast<jclass>(env->NewGlobalRef(obj_class));
	}

	virtual ~JNIBbqueEXC() {
		delete jvm;
	}

private:

	JavaVM *jvm;
	jobject callback_obj;
	jclass callback_class;

	RTLIB_ExitCode_t onSetup() override {
		return onGenericIntCallback("onSetupCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onConfigure(int8_t awm_id) override {
		return onGenericIntCallback("onConfigureCallback", "(I)Lbbque/rtlib/enumeration/RTLibExitCode;", (jint) awm_id);
	}

	RTLIB_ExitCode_t onSuspend() override {
		return onGenericIntCallback("onSuspendCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onResume() override {
		return onGenericIntCallback("onResumeCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onRun() override {
		return onGenericIntCallback("onRunCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onMonitor() override {
		return onGenericIntCallback("onMonitorCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onRelease() override {
		return onGenericIntCallback("onReleaseCallback", "()Lbbque/rtlib/enumeration/RTLibExitCode;");
	}

	RTLIB_ExitCode_t onGenericIntCallback(const char *method, const char *signature, ...);
};

#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    initNative
 * Signature: (Ljava/lang/String;Ljava/lang/String;Lbbque/rtlib/model/RTLibServices;)J
 */
JNIEXPORT jlong JNICALL Java_bbque_rtlib_model_BbqueEXC_initNative
  (JNIEnv *, jobject, jstring, jstring, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    isRegistered
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_bbque_rtlib_model_BbqueEXC_isRegistered
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_start
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    waitCompletion
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_waitCompletion
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    terminate
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_terminate
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    enable
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_enable
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    disable
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_disable
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setAWMConstraints
 * Signature: ([Lbbque/rtlib/model/RTLibConstraint;)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setAWMConstraints
  (JNIEnv *, jobject, jobjectArray);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    clearAWMConstraints
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_clearAWMConstraints
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setGoalGap
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setGoalGap
  (JNIEnv *, jobject, jint);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getUniqueID_String
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_bbque_rtlib_model_BbqueEXC_getUniqueID_1String
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getUniqueID
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_getUniqueID
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getAssignedResources
 * Signature: (Lbbque/rtlib/enumeration/RTLibResourceType;)I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_getAssignedResources__Lbbque_rtlib_enumeration_RTLibResourceType_2
  (JNIEnv *, jobject, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getAffinityMask
 * Signature: ([I)[I
 */
JNIEXPORT jintArray JNICALL Java_bbque_rtlib_model_BbqueEXC_getAffinityMask
  (JNIEnv *, jobject, jintArray);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getAssignedResources
 * Signature: (Lbbque/rtlib/enumeration/RTLibResourceType;[I)[I
 */
JNIEXPORT jintArray JNICALL Java_bbque_rtlib_model_BbqueEXC_getAssignedResources__Lbbque_rtlib_enumeration_RTLibResourceType_2_3I
  (JNIEnv *, jobject, jobject, jintArray);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setCPS
 * Signature: (F)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setCPS
  (JNIEnv *, jobject, jfloat);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setJPSGoal
 * Signature: (FFI)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setJPSGoal
  (JNIEnv *, jobject, jfloat, jfloat, jint);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    updateJPC
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_updateJPC
  (JNIEnv *, jobject, jint);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setCPSGoal
 * Signature: (FF)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setCPSGoal__FF
  (JNIEnv *, jobject, jfloat, jfloat);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setCPSGoal
 * Signature: (F)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setCPSGoal__F
  (JNIEnv *, jobject, jfloat);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    setMinimumCycleTimeUs
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_bbque_rtlib_model_BbqueEXC_setMinimumCycleTimeUs
  (JNIEnv *, jobject, jint);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getCPS
 * Signature: ()F
 */
JNIEXPORT jfloat JNICALL Java_bbque_rtlib_model_BbqueEXC_getCPS
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getJPS
 * Signature: ()F
 */
JNIEXPORT jfloat JNICALL Java_bbque_rtlib_model_BbqueEXC_getJPS
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getCTimeUs
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_getCTimeUs
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    getCTimeMs
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_getCTimeMs
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    cycles
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_cycles
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    workingModeParams
 * Signature: ()Lbbque/rtlib/model/RTLibWorkingModeParams;
 */
JNIEXPORT jobject JNICALL Java_bbque_rtlib_model_BbqueEXC_workingModeParams
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    done
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_bbque_rtlib_model_BbqueEXC_done
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    currentAWM
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_bbque_rtlib_model_BbqueEXC_currentAWM
  (JNIEnv *, jobject);

/*
 * Class:     bbque_rtlib_model_BbqueEXC
 * Method:    configuration
 * Signature: ()Lbbque/rtlib/model/RTLibConfig;
 */
JNIEXPORT jobject JNICALL Java_bbque_rtlib_model_BbqueEXC_configuration
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif

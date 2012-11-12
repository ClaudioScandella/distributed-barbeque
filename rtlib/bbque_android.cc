/*
 * Copyright (C) 2012  Politecnico di Milano
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "bbque/rtlib.h"
#include "bbque/bbque_exc.h"

#include <jni.h>
#include <android/log.h>

#define LOG_TAG "BbqueWrapper"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__ )
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__ )
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__ )


/**
 * @brief Global reference to the caller Java VM
 */
static JavaVM *jvm = NULL;

/**
 * @brief Global reference to the (java side) BbqueService
 *
 * Global reference to the caller Object, so that the garbage collector won't
 * delete it.
 */
static jobject obj;

/**
 * @brief Global reference to the RTLib instance
 */
RTLIB_Services_t *rtlib = NULL;

jint JNI_OnLoad(JavaVM* _jvm, void* _reserved) {
    jvm = _jvm;
    JNIEnv *env;

    if (jvm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
	    LOGE("Failed to get the environment using GetEnv()");
	    return -1;
    }

    LOGI("JNI_OnLoad completed");
    return JNI_VERSION_1_4;
}

extern "C" {

//========== rtlib.h
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_RTLIBInit(
		JNIEnv *_env, jobject _thiz,
		jstring _name);
JNIEXPORT void
Java_it_polimi_dei_bosp_BbqueService_RTLIBExit(
		JNIEnv *_env, jobject _thiz);

//========== wrapper commodities
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCCreate(
		JNIEnv *_env, jobject _thiz,
		jstring _name,
		jstring _recipe);

//========== bbque_exc.h

JNIEXPORT jboolean
Java_it_polimi_dei_bosp_BbqueService_EXCisRegistered(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCStart(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCWaitCompletion(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCTerminate(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCEnable(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCDisable(
		JNIEnv *_env, jobject _thiz);


////RTLIB_ExitCode_t SetConstraints(RTLIB_Constraint_t *constraints, uint8_t count);
//JNIEXPORT jint
// Java_it_polimi_dei_bosp_BbqueService_EXC_
//		JNIEnv *_env, jobject _thiz);
////RTLIB_ExitCode_t ClearConstraints();
//JNIEXPORT jint
//Java_it_polimi_dei_bosp_BbqueService_EXC_
//		JNIEnv *_env, jobject _thiz);
////RTLIB_ExitCode_t SetGoalGap(uint8_t percent);
//JNIEXPORT jint
//Java_it_polimi_dei_bosp_BbqueService_EXC_
//		JNIEnv *_env, jobject _thiz);
////
////RTLIB_WorkingModeParams_t const & WorkingModeParams() const;
//JNIEXPORT jint
//Java_it_polimi_dei_bosp_BbqueService_EXC_
//		JNIEnv *_env, jobject _thiz);


JNIEXPORT jstring
Java_it_polimi_dei_bosp_BbqueService_EXCGetChUid(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jlong
Java_it_polimi_dei_bosp_BbqueService_EXCGetUid(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCSetCPS(
		JNIEnv *_env, jobject _thiz, jfloat _cps);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCSetCTimeUs(
		JNIEnv *_env, jobject _thiz, jint us);

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCCycles(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jboolean
Java_it_polimi_dei_bosp_BbqueService_EXCDone(
		JNIEnv *_env, jobject _thiz);

JNIEXPORT jbyte
Java_it_polimi_dei_bosp_BbqueService_EXCCurrentAWM(
		JNIEnv *_env, jobject _thiz);

}; // extern "C"

// RTLIB_ExitCode_t RTLIB_Init(const char *name, RTLIB_Services_t **rtlib)
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_RTLIBInit(
		JNIEnv *_env, jobject _thiz,
		jstring _name) {
	const char *name = _env->GetStringUTFChars(_name, 0);
	RTLIB_ExitCode_t result;

	obj = (jobject)_env->NewGlobalRef(_thiz);

	result = RTLIB_Init(name, &rtlib);
	if (result != RTLIB_OK) {
		LOGE("RTLIB initialization failed");
		return (-result);
	}

	LOGI("RTLIB initialization done");
	return RTLIB_OK;
}

//extern void RTLIB_Exit(void);
// static void RTLIB_Exit(void)
JNIEXPORT void
Java_it_polimi_dei_bosp_BbqueService_RTLIBExit(JNIEnv *_env, jobject _thiz) {
	LOGI("RTLIB destruction...");
	// RTLIB_Exit();
}

using bbque::rtlib::BbqueEXC;
class BbqueAndroid : public BbqueEXC {

public:
	BbqueAndroid(std::string const & name,
			std::string const & recipe,
			RTLIB_Services_t *rtlib);
	RTLIB_ExitCode_t onSetup();
	RTLIB_ExitCode_t onConfigure(uint8_t awm_id);
	RTLIB_ExitCode_t onSuspend();
	RTLIB_ExitCode_t onResume();
	RTLIB_ExitCode_t onRun();
	RTLIB_ExitCode_t onMonitor();
	RTLIB_ExitCode_t onRelease();

private:
	JNIEnv *env;
	bool attached;

	typedef struct {
		const char* name;
		const char* signature;
		jmethodID method;
	} callback_t;

	typedef enum {
		ON_SETUP = 0,
		ON_CONFIGURE,
		ON_SUSPEND,
		ON_RESUME,
		ON_RUN,
		ON_MONITOR,
		ON_RELEASE,
		CB_COUNT // This must be the last entry
	} cbid_t;

	static callback_t cb[CB_COUNT];

};

BbqueAndroid::callback_t BbqueAndroid::cb[CB_COUNT] = {
	{"onSetup", 	"()I", 	0},
	{"onConfigure",	"(I)I", 0},
	{"onSuspend", 	"()I", 	0},
	{"onResume", 	"()I", 	0},
	{"onRun", 	"()I", 	0},
	{"onMonitor", 	"()I", 	0},
	{"onRelease", 	"()I", 	0},
};


BbqueAndroid::BbqueAndroid(std::string const & name,
	std::string const & recipe,
	RTLIB_Services_t *rtlib) :
	BbqueEXC(name, recipe, rtlib),
	env(NULL),
	attached(false) {

	LOGD("BbqueAndroid()");

}


RTLIB_ExitCode_t
BbqueAndroid::onSetup() {
	jclass clazz;
	int status;
	uint8_t i;

	LOGI("Attach JVM environment from RTLib thread...");
	status = jvm->GetEnv((void **) &env, JNI_VERSION_1_4);
	if (status < 0) {
		LOGE("Failed to get JNI environment, assuming native thread");
		status = jvm->AttachCurrentThread(&env, NULL);
		if (status < 0) {
			LOGE("Failed to attach current thread");
			return RTLIB_ERROR;
		}
		attached = true;
	}

	if (env == NULL) {
		LOGE("Failed to get JNI environment");
		return RTLIB_ERROR;
	}

	LOGI("Keep track of callbacks signatures...");
	clazz = env->GetObjectClass(obj);
	for (i = 0; i < CB_COUNT; ++i) {
		cb[i].method = env->GetMethodID(clazz, cb[i].name, cb[i].signature);
		if (cb[i].method == NULL)
			break;
	}
	if (i < CB_COUNT) {
		LOGE("Failed to get all callbacks method IDs");
		return RTLIB_ERROR;
	}

	// Forward callback to application specific setup
	LOGD("Callback onSetup()");
	if (env->CallIntMethod(obj, cb[ON_SETUP].method))
		return RTLIB_ERROR;

	return RTLIB_OK;
}

RTLIB_ExitCode_t
BbqueAndroid::onConfigure(uint8_t awm_id) {
	LOGD("Callback onConfigure(%d)", awm_id);
	if (env->CallIntMethod(obj, cb[ON_CONFIGURE].method, (int)awm_id))
		return RTLIB_ERROR;

	return RTLIB_OK;
}

RTLIB_ExitCode_t
BbqueAndroid::onSuspend() {
	LOGD("Callback onSuspsend");
	if (env->CallIntMethod(obj, cb[ON_SUSPEND].method))
		return RTLIB_ERROR;

	return RTLIB_OK;
}

RTLIB_ExitCode_t
BbqueAndroid::onResume() {
	LOGD("Callback onResume()");
	if (env->CallIntMethod(obj, cb[ON_RESUME].method))
		return RTLIB_ERROR;

	return RTLIB_OK;
}

RTLIB_ExitCode_t
BbqueAndroid::onRun() {
	LOGD("Callback onRun(), %d", Cycles());
//#define TEST_JNI
#ifndef TEST_JNI
	if (env->CallIntMethod(obj, cb[ON_RUN].method))
		return RTLIB_EXC_WORKLOAD_NONE;

	return RTLIB_OK;
#else
	::usleep(1000000);
	if (Cycles() > 5)
		return RTLIB_EXC_WORKLOAD_NONE;
	return RTLIB_OK;
#endif
}

RTLIB_ExitCode_t
BbqueAndroid::onMonitor() {
	LOGD("Callback onMonitor()");
	if (env->CallIntMethod(obj, cb[ON_MONITOR].method))
		return RTLIB_ERROR;

	return RTLIB_OK;
}

RTLIB_ExitCode_t
BbqueAndroid::onRelease() {
	LOGD("Callback onRelease()");
	if (env->CallIntMethod(obj, cb[ON_RELEASE].method))
		return RTLIB_ERROR;

	// Detaching JVM thread
	if (attached)
		jvm->DetachCurrentThread();
	return RTLIB_OK;
}



BbqueEXC *exc = NULL;

JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCCreate(
	JNIEnv *_env, jobject _thiz,
	jstring _name,
	jstring _recipe) {
	const char *name   = _env->GetStringUTFChars(_name, 0);
	const char *recipe = _env->GetStringUTFChars(_recipe, 0);

	LOGI("Building new EXC...");
	exc = new BbqueAndroid(name, recipe, rtlib);
	if (!exc || !exc->isRegistered()) {
		LOGE("building new EXC FAILED!");
		return -RTLIB_ERROR;
	}

	LOGI("Building new EXC... SUCCESS");
	return RTLIB_OK;
}


//inline bool isRegistered() const;
JNIEXPORT jboolean
Java_it_polimi_dei_bosp_BbqueService_EXCisRegistered(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call isRegistered()");
	if (!exc) return false;
	return exc->isRegistered();
}

//RTLIB_ExitCode_t Start();
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCStart(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Start()");
	return exc->Start();
}

//RTLIB_ExitCode_t WaitCompletion();
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCWaitCompletion(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call WaitCompletion()");
	return exc->WaitCompletion();
}

//RTLIB_ExitCode_t Terminate();
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCTerminate(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Terminate()");
	return exc->Terminate();
}

//RTLIB_ExitCode_t Enable();
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCEnable(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Enable()");
	return exc->Enable();
}

//RTLIB_ExitCode_t Disable();
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCDisable(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Disable()");
	return exc->Disable();
}

//const char *GetChUid() const;
JNIEXPORT jstring
Java_it_polimi_dei_bosp_BbqueService_EXCGetChUid(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call GetChUid()");
	// FIXME this could be subject to leackage!!!
	return _env->NewStringUTF(exc->GetChUid());
}

//inline AppUid_t GetUid() const;
JNIEXPORT jlong
Java_it_polimi_dei_bosp_BbqueService_EXCGetUid(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call GetUid()");
	return exc->GetUid();
}

//RTLIB_ExitCode_t SetCPS(float cps);
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCSetCPS(
		JNIEnv *_env, jobject _thiz, jfloat _cps) {
	LOGD("Forwarding call SetCPS()");
	return exc->SetCPS(_cps);
}

//RTLIB_ExitCode_t SetCTimeUs(uint32_t us);
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCSetCTimeUs(
		JNIEnv *_env, jobject _thiz, jint _us) {
	LOGD("Forwarding call SetCTimeUs");
	return exc->SetCTimeUs(_us);
}

//inline uint32_t Cycles() const;
JNIEXPORT jint
Java_it_polimi_dei_bosp_BbqueService_EXCCycles(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Cycles()");
	return exc->Cycles();
}

//inline bool Done() const;
JNIEXPORT jboolean
Java_it_polimi_dei_bosp_BbqueService_EXCDone(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call Done()");
	return exc->Done();
}

//int8_t CurrentAWM() const;
JNIEXPORT jbyte
Java_it_polimi_dei_bosp_BbqueService_EXCCurrentAWM(
		JNIEnv *_env, jobject _thiz) {
	LOGD("Forwarding call CurrentAWM()");
	return exc->CurrentAWM();
}

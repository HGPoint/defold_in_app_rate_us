#include "inapp_review_private.h"

#if defined(DM_PLATFORM_ANDROID)

#include <dmsdk/sdk.h>
#include <dmsdk/dlib/android.h>

namespace
{
    static const char* JAVA_CLASS = "com.defold.android.inappreview.InAppReview";
    static jclass g_Class = 0;
    static jmethodID g_IsSupported = 0;
    static jmethodID g_Request = 0;
    static jmethodID g_Reset = 0;

    static bool ClearException(JNIEnv* env, const char* action)
    {
        if (!env || !env->ExceptionCheck())
            return false;
        dmLogError("Java exception while %s", action);
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }

    static void ResetBindings(JNIEnv* env)
    {
        if (env && g_Class)
            env->DeleteGlobalRef(g_Class);
        g_Class = 0;
        g_IsSupported = 0;
        g_Request = 0;
        g_Reset = 0;
    }

    static bool LoadMethod(JNIEnv* env, jmethodID* output, const char* name, const char* signature)
    {
        *output = env->GetStaticMethodID(g_Class, name, signature);
        const bool had_exception = ClearException(env, name);
        return *output != 0 && !had_exception;
    }

    static bool EnsureBindings(JNIEnv* env)
    {
        if (g_Class)
            return true;
        if (!env)
            return false;

        jclass local = dmAndroid::LoadClass(env, JAVA_CLASS);
        const bool load_exception = ClearException(env, "loading InAppReview");
        if (!local || load_exception)
        {
            if (local)
                env->DeleteLocalRef(local);
            return false;
        }

        g_Class = (jclass)env->NewGlobalRef(local);
        env->DeleteLocalRef(local);
        if (ClearException(env, "creating InAppReview global reference") || !g_Class)
        {
            ResetBindings(env);
            return false;
        }

        const bool loaded =
            LoadMethod(env, &g_IsSupported, "IsSupported", "(Landroid/app/Activity;)Z") &&
            LoadMethod(env, &g_Request, "Request", "(Landroid/app/Activity;I)I") &&
            LoadMethod(env, &g_Reset, "Reset", "()V");
        if (!loaded)
            ResetBindings(env);
        return loaded;
    }
}

extern "C" JNIEXPORT void JNICALL Java_com_defold_android_inappreview_InAppReview_nativeOnResult(
    JNIEnv*, jclass, jint request_id, jint completion)
{
    if (completion < INAPP_REVIEW_REQUESTED || completion > INAPP_REVIEW_COMPLETION_NOT_VISIBLE)
        completion = INAPP_REVIEW_REQUEST_FAILED;
    InAppReview_Complete((int)request_id, (InAppReviewCompletion)completion);
}

bool InAppReviewPlatform_IsSupported()
{
    dmAndroid::ThreadAttacher attacher;
    JNIEnv* env = attacher.GetEnv();
    if (!EnsureBindings(env))
        return false;
    jobject activity = dmGraphics::GetNativeAndroidActivity();
    if (!activity)
        return false;

    const jboolean supported = env->CallStaticBooleanMethod(g_Class, g_IsSupported, activity);
    return !ClearException(env, "checking review support") && supported == JNI_TRUE;
}

InAppReviewResult InAppReviewPlatform_Request(int request_id)
{
    dmAndroid::ThreadAttacher attacher;
    JNIEnv* env = attacher.GetEnv();
    if (!EnsureBindings(env))
        return INAPP_REVIEW_PLATFORM_ERROR;
    jobject activity = dmGraphics::GetNativeAndroidActivity();
    if (!activity)
        return INAPP_REVIEW_PLATFORM_ERROR;

    const jint result = env->CallStaticIntMethod(g_Class, g_Request, activity, (jint)request_id);
    if (ClearException(env, "requesting in-app review"))
        return INAPP_REVIEW_PLATFORM_ERROR;
    if (result < INAPP_REVIEW_OK || result > INAPP_REVIEW_PLATFORM_ERROR)
        return INAPP_REVIEW_PLATFORM_ERROR;
    return (InAppReviewResult)result;
}

void InAppReviewPlatform_Finalize()
{
    if (!g_Class)
        return;
    dmAndroid::ThreadAttacher attacher;
    JNIEnv* env = attacher.GetEnv();
    if (env)
    {
        env->CallStaticVoidMethod(g_Class, g_Reset);
        ClearException(env, "resetting in-app review");
    }
    ResetBindings(env);
}

#endif

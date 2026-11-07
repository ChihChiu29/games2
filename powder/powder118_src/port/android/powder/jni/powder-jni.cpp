/*
 * PROPRIETARY INFORMATION.  This software is proprietary to POWDER
 * Development, and is not to be reproduced, transmitted, or disclosed
 * in any way without written permission.
 *
 * Produced by:	Jeff Lait
 *
 *      	POWDER Development
 *
 * NAME:        powder-jni.cpp ( POWDER Library, C++ )
 *
 * COMMENTS:
 *	This provides the interface between Java and C++
 */

#include <string.h>
#include <jni.h>

#include "mygba.h"
#include "hamfake.h"

extern bool android_main(const char *path);

#define PREFIX(x) Java_com_zincland_powder_PowderActivity_##x

extern "C" {
JNIEXPORT jboolean JNICALL PREFIX(startPowderThreadJNI)(JNIEnv *env, jobject thiz, jstring path);
JNIEXPORT void JNICALL PREFIX(vblJNI)(JNIEnv *env);
JNIEXPORT void JNICALL PREFIX(forceSaveJNI)(JNIEnv *env);

JNIEXPORT void JNICALL PREFIX(setStylusPosJNI)(JNIEnv *env, jobject thiz, jboolean state, jint x, jint y);

JNIEXPORT jintArray JNICALL PREFIX(getFrameBufferJNI)(JNIEnv *env, jobject thiz, jintArray oldarr);
JNIEXPORT jint JNICALL PREFIX(getFrameWidthJNI)(JNIEnv *env);
JNIEXPORT jint JNICALL PREFIX(getFrameHeightJNI)(JNIEnv *env);

JNIEXPORT jint JNICALL PREFIX(pollButtonReqJNI)(JNIEnv *env);
JNIEXPORT void JNICALL PREFIX(postInputStringJNI)(JNIEnv *env, jobject thiz, jstring text);

JNIEXPORT void JNICALL PREFIX(postDirJNI)(JNIEnv *env, jobject thiz, jint dx, jint dy);
JNIEXPORT void JNICALL PREFIX(setFakeButtonJNI)(JNIEnv *env, jobject thiz, jint button, jboolean state);
JNIEXPORT void JNICALL PREFIX(postOrientationJNI)(JNIEnv *env, jobject thiz, jboolean isportrait);
JNIEXPORT void JNICALL PREFIX(revertDefaultsJNI)(JNIEnv *env, jobject thiz);
};

static int glbJNI_FrameWidth, glbJNI_FrameHeight;

JNIEXPORT jboolean JNICALL PREFIX(startPowderThreadJNI)(JNIEnv *env, jobject thiz, jstring path)
{
    const char *localtext = env->GetStringUTFChars(path, NULL);

    bool reused = android_main(localtext);

    env->ReleaseStringUTFChars(path, localtext);

    return reused;
}

JNIEXPORT void JNICALL PREFIX(vblJNI)(JNIEnv *env)
{
    hamfake_callThisFromVBL();
}

void hamfake_awaitShutdown();

JNIEXPORT void JNICALL PREFIX(forceSaveJNI)(JNIEnv *env)
{
    hamfake_setForceQuit();
    hamfake_awaitShutdown();
}

jint		*glbFill;
int		 glbLastFillSize;

JNIEXPORT jintArray JNICALL PREFIX(getFrameBufferJNI)(JNIEnv *env, jobject thiz, jintArray oldarr)
{
    jintArray	result;
    SCREENDATA	screen;

    hamfake_getActualScreen(screen);

    const u8	*src = screen.data();
    int		 pixelcount = screen.width() * screen.height();

    if (!glbFill || glbLastFillSize != pixelcount)
    {
	delete [] glbFill;
	glbFill = new jint[pixelcount];
	glbLastFillSize = pixelcount;
    }

    int i = 0;
    for (int y = 0; y < screen.height(); y++)
    {
	for (int x = 0; x < screen.width(); x++)
	{
	    int		val;
	    val = 255;
	    val <<= 8;
	    val |= *src++;
	    val <<= 8;
	    val |= *src++;
	    val <<= 8;
	    val |= *src++;
	    glbFill[i++] = val;
	}
    }

    // See if the old array is big enough.
    jsize oldsize = (*env).GetArrayLength(oldarr);

    if (oldsize == pixelcount)
    {
#if 0
	(*env).SetIntArrayRegion(oldarr, 0, pixelcount, glbFill);
#else
	jint *rawarr = (*env).GetIntArrayElements(oldarr, 0);
	memcpy(rawarr, glbFill, sizeof(jint) * pixelcount);
	(*env).ReleaseIntArrayElements(oldarr, rawarr, 0);
#endif
	result = (*env).NewIntArray(0);
    }
    else
    {
	result = (*env).NewIntArray(pixelcount);

	(*env).SetIntArrayRegion(result, 0, pixelcount, glbFill);
    }

    glbJNI_FrameWidth = screen.width();
    glbJNI_FrameHeight = screen.height();

    return result;
}

JNIEXPORT jint JNICALL PREFIX(getFrameWidthJNI)(JNIEnv *env)
{
    jint	w = glbJNI_FrameWidth;

    return w;
}
JNIEXPORT jint JNICALL PREFIX(getFrameHeightJNI)(JNIEnv *env)
{
    jint	h = glbJNI_FrameHeight;

    return h;
}

JNIEXPORT void JNICALL PREFIX(setStylusPosJNI)(JNIEnv *env, jobject thiz, jboolean jstate, jint jx, jint jy)
{
    int			x, y;
    bool		state;
    x = jx;
    y = jy;
    state = jstate;

    hamfake_setstyluspos(state, x, y);
}

JNIEXPORT jint JNICALL PREFIX(pollButtonReqJNI)(JNIEnv *env)
{
    int		mode, type;
    bool	state = hamfake_getbuttonreq(mode, type);

    if (!state)
	return -1;

    return (mode << 16) | type;
}

extern void hamfake_finishexternalinput();

JNIEXPORT void JNICALL PREFIX(postInputStringJNI)(JNIEnv *env, jobject thiz, jstring text)
{
    const char *localtext = env->GetStringUTFChars(text, NULL);
    strncpy(glbInputData.myText, localtext, glbInputData.myMaxLen);
    glbInputData.myText[glbInputData.myMaxLen] = 0;
    env->ReleaseStringUTFChars(text, localtext);
    hamfake_finishexternalinput();
}

JNIEXPORT void JNICALL PREFIX(postDirJNI)(JNIEnv *env, jobject thiz, jint dx, jint dy)
{
    hamfake_postdir(dx, dy);
}

JNIEXPORT void JNICALL PREFIX(postOrientationJNI)(JNIEnv *env, jobject thiz, jboolean isportrait)
{
    hamfake_postorientation(isportrait);
}

JNIEXPORT void JNICALL PREFIX(revertDefaultsJNI)(JNIEnv *env, jobject thiz)
{
    hamfake_revertdefault();
}

JNIEXPORT void JNICALL PREFIX(setFakeButtonJNI)(JNIEnv *env, jobject thiz, jint button, jboolean state)
{
    hamfake_setFakeButtonState( (FAKE_BUTTONS) button, state);
}

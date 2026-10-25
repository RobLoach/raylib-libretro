/**********************************************************************************************
*
*   raylib-libretro-android - Android integrations for raylib-libretro.
*
*   LICENSE: GPL-3.0-or-later
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_ANDROID_H
#define RAYLIB_LIBRETRO_ANDROID_H

#if defined(__ANDROID__)

#include <jni.h>
#include <stdio.h>
#include <stdarg.h>
#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);
static bool AndroidGetExternalStorageDir(struct android_app* app, char* out);

#ifndef RAYLIB_LIBRETRO_ANDROID_ABI
    #if defined(__aarch64__)
        #define RAYLIB_LIBRETRO_ANDROID_ABI "arm64-v8a"
    #elif defined(__arm__)
        #define RAYLIB_LIBRETRO_ANDROID_ABI "armeabi-v7a"
    #elif defined(__x86_64__)
        #define RAYLIB_LIBRETRO_ANDROID_ABI "x86_64"
    #elif defined(__i386__)
        #define RAYLIB_LIBRETRO_ANDROID_ABI "x86"
    #else
        #define RAYLIB_LIBRETRO_ANDROID_ABI ""
    #endif
#endif

#endif /* __ANDROID__ */
#endif /* RAYLIB_LIBRETRO_ANDROID_H */

/**********************************************************************************************
*   Implementation
**********************************************************************************************/

#if defined(RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION) && !defined(RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION_ONCE)
#define RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION_ONCE

#if defined(__ANDROID__)

static FILE* AndroidLogFile = NULL;

static void AndroidTraceLog(int logLevel, const char* text, va_list args) {
    int prio = ANDROID_LOG_INFO;
    switch (logLevel) {
        case LOG_TRACE:   prio = ANDROID_LOG_VERBOSE; break;
        case LOG_DEBUG:   prio = ANDROID_LOG_DEBUG;   break;
        case LOG_INFO:    prio = ANDROID_LOG_INFO;    break;
        case LOG_WARNING: prio = ANDROID_LOG_WARN;    break;
        case LOG_ERROR:   prio = ANDROID_LOG_ERROR;   break;
        case LOG_FATAL:   prio = ANDROID_LOG_FATAL;   break;
        default: break;
    }
    va_list fileArgs;
    va_copy(fileArgs, args);
    __android_log_vprint(prio, "raylib-libretro", text, args);
    if (AndroidLogFile != NULL) {
        vfprintf(AndroidLogFile, text, fileArgs);
        fputc('\n', AndroidLogFile);
        fflush(AndroidLogFile);
    }
    va_end(fileArgs);
}

static void AndroidInitCrashLog(struct android_app* app) {
    if (app == NULL || app->activity == NULL) return;
    const char* dir = app->activity->externalDataPath;
    if (dir == NULL) dir = app->activity->internalDataPath;
    if (dir == NULL) return;
    const char* path = TextFormat("%s/raylib-libretro.log", dir);
    AndroidLogFile = fopen(path, "w");
    if (AndroidLogFile != NULL) {
        SetTraceLogCallback(AndroidTraceLog);
        TraceLog(LOG_INFO, "ANDROID: Crash log at %s", path);
    }
}

static JNIEnv* AndroidJNIBegin(struct android_app* app, bool* outNeedDetach) {
    *outNeedDetach = false;
    if (app == NULL || app->activity == NULL || app->activity->vm == NULL) return NULL;
    JavaVM* vm = app->activity->vm;
    JNIEnv* env = NULL;
    jint res = (*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != 0 || env == NULL) return NULL;
        *outNeedDetach = true;
    } else if (res != JNI_OK || env == NULL) {
        return NULL;
    }
    return env;
}

static void AndroidJNIEnd(struct android_app* app, bool needDetach) {
    if (needDetach && app != NULL && app->activity != NULL && app->activity->vm != NULL) {
        (*app->activity->vm)->DetachCurrentThread(app->activity->vm);
    }
}

static void AndroidCopyCore(const char* coreName, const char* destDir) {
    char destPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    char assetPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];
    TextCopy(destPath, TextFormat("%s/%s", destDir, coreName));
    TextCopy(assetPath, TextFormat("cores/%s/%s", RAYLIB_LIBRETRO_ANDROID_ABI, coreName));

    if (FileExists(destPath)) return;

    int size = 0;
    unsigned char* data = LoadFileData(assetPath, &size);
    if (data == NULL || size <= 0) {
        TraceLog(LOG_WARNING, "ANDROID: Bundled core missing from assets: %s", assetPath);
        return;
    }
    if (SaveFileData(destPath, data, size)) {
        TraceLog(LOG_INFO, "ANDROID: Installed core %s", destPath);
    } else {
        TraceLog(LOG_ERROR, "ANDROID: Failed to install core %s", destPath);
    }
    UnloadFileData(data);
}

static void AndroidRequestStorageAccess(struct android_app* app) {
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (env == NULL) {
        TraceLog(LOG_WARNING, "ANDROID: Could not attach JNI for storage access");
        return;
    }

    jint sdkInt = 0;
    jclass versionClass = (*env)->FindClass(env, "android/os/Build$VERSION");
    if (versionClass != NULL) {
        jfieldID sdkIntField = (*env)->GetStaticFieldID(env, versionClass, "SDK_INT", "I");
        if (sdkIntField != NULL) sdkInt = (*env)->GetStaticIntField(env, versionClass, sdkIntField);
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    if (sdkInt >= 30) {
        jboolean granted = JNI_TRUE;
        jclass environmentClass = (*env)->FindClass(env, "android/os/Environment");
        if (environmentClass != NULL) {
            jmethodID isManager = (*env)->GetStaticMethodID(env, environmentClass,
                "isExternalStorageManager", "()Z");
            if (isManager != NULL) {
                granted = (*env)->CallStaticBooleanMethod(env, environmentClass, isManager);
            }
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

        if (!granted) {
            jclass  activityClass   = (*env)->GetObjectClass(env, app->activity->clazz);
            jmethodID getPackageName = activityClass ? (*env)->GetMethodID(env, activityClass,
                "getPackageName", "()Ljava/lang/String;") : NULL;
            jstring packageName = getPackageName ?
                (jstring)(*env)->CallObjectMethod(env, app->activity->clazz, getPackageName) : NULL;

            jclass  uriClass = (*env)->FindClass(env, "android/net/Uri");
            jmethodID uriParse = uriClass ? (*env)->GetStaticMethodID(env, uriClass, "parse",
                "(Ljava/lang/String;)Landroid/net/Uri;") : NULL;

            jclass  settingsClass = (*env)->FindClass(env, "android/provider/Settings");
            jfieldID actionField = settingsClass ? (*env)->GetStaticFieldID(env, settingsClass,
                "ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION", "Ljava/lang/String;") : NULL;
            jstring action = actionField ?
                (jstring)(*env)->GetStaticObjectField(env, settingsClass, actionField) : NULL;

            jclass  intentClass = (*env)->FindClass(env, "android/content/Intent");
            jmethodID intentInit = intentClass ? (*env)->GetMethodID(env, intentClass, "<init>",
                "(Ljava/lang/String;Landroid/net/Uri;)V") : NULL;
            jmethodID startActivity = activityClass ? (*env)->GetMethodID(env, activityClass,
                "startActivity", "(Landroid/content/Intent;)V") : NULL;

            if (packageName && uriParse && action && intentInit && startActivity) {
                const char* packageUtf = (*env)->GetStringUTFChars(env, packageName, NULL);
                jstring uriString = (*env)->NewStringUTF(env,
                    TextFormat("package:%s", packageUtf ? packageUtf : ""));
                if (packageUtf) (*env)->ReleaseStringUTFChars(env, packageName, packageUtf);

                jobject uri = (*env)->CallStaticObjectMethod(env, uriClass, uriParse, uriString);
                jobject intent = uri ? (*env)->NewObject(env, intentClass, intentInit, action, uri) : NULL;
                if (intent != NULL) {
                    (*env)->CallVoidMethod(env, app->activity->clazz, startActivity, intent);
                    TraceLog(LOG_INFO, "ANDROID: Requested All-Files Access for ROM browsing");
                }
            } else {
                TraceLog(LOG_WARNING, "ANDROID: Could not build All-Files Access intent");
            }
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        }
    }

    AndroidJNIEnd(app, needDetach);
}

static void AndroidEnableImmersiveMode(struct android_app* app) {
    if (app == NULL || app->activity == NULL) return;
    ANativeActivity_setWindowFlags(app->activity,
        AWINDOW_FLAG_FULLSCREEN | AWINDOW_FLAG_KEEP_SCREEN_ON, 0);
}

// Resolve the user-visible shared storage root (e.g. /storage/emulated/0), where
// ROMs typically live, via Environment.getExternalStorageDirectory(). Querying the
// path needs no permission; reading its contents requires MANAGE_EXTERNAL_STORAGE
// (requested by AndroidRequestStorageAccess before the user browses).
static bool AndroidGetExternalStorageDir(struct android_app* app, char* out) {
    out[0] = '\0';
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (env == NULL) return false;

    bool result = false;
    jclass environmentClass = (*env)->FindClass(env, "android/os/Environment");
    jmethodID getDir = environmentClass ? (*env)->GetStaticMethodID(env, environmentClass,
        "getExternalStorageDirectory", "()Ljava/io/File;") : NULL;
    jobject file = getDir ?
        (*env)->CallStaticObjectMethod(env, environmentClass, getDir) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); file = NULL; }

    if (file != NULL) {
        jclass fileClass = (*env)->GetObjectClass(env, file);
        jmethodID getPath = (*env)->GetMethodID(env, fileClass,
            "getAbsolutePath", "()Ljava/lang/String;");
        jstring path = getPath ? (jstring)(*env)->CallObjectMethod(env, file, getPath) : NULL;
        const char* pathUtf = path ? (*env)->GetStringUTFChars(env, path, NULL) : NULL;
        if (pathUtf != NULL && pathUtf[0] != '\0') {
            TextCopy(out, pathUtf);
            result = true;
        }
        if (pathUtf) (*env)->ReleaseStringUTFChars(env, path, pathUtf);
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    AndroidJNIEnd(app, needDetach);
    return result;
}

static void SetupAndroidEnvironment(LibretroMenu* menu) {
    struct android_app* app = GetAndroidApp();
    if (app == NULL || app->activity == NULL || app->activity->internalDataPath == NULL) {
        TraceLog(LOG_WARNING, "ANDROID: No activity data path; skipping environment setup");
        return;
    }
    const char* dataDir = app->activity->internalDataPath;
    const char* externalDir = app->activity->externalDataPath;

    if (LIBRETRO.coreDirectory[0]   != '/') TextCopy(LIBRETRO.coreDirectory,   TextFormat("%s/cores", dataDir));
    if (LIBRETRO.saveDirectory[0]   != '/') TextCopy(LIBRETRO.saveDirectory,   TextFormat("%s/saves", dataDir));
    if (LIBRETRO.systemDirectory[0] != '/') TextCopy(LIBRETRO.systemDirectory, TextFormat("%s/system", dataDir));
    if (LIBRETRO.fileBrowserStartDirectory[0] == '\0') {
        // Default the file browser to the shared storage root (where ROMs live),
        // falling back to the app-private external dir if JNI is unavailable.
        if (!AndroidGetExternalStorageDir(app, LIBRETRO.fileBrowserStartDirectory) && externalDir != NULL) {
            TextCopy(LIBRETRO.fileBrowserStartDirectory, externalDir);
        }
    }

    MakeDirectory(LIBRETRO.coreDirectory);
    MakeDirectory(LIBRETRO.saveDirectory);
    MakeDirectory(LIBRETRO.systemDirectory);

    char* list = LoadFileText(TextFormat("cores/%s/cores.list", RAYLIB_LIBRETRO_ANDROID_ABI));
    if (list != NULL) {
        int count = 0;
        char** lines = TextSplit(list, '\n', &count);
        for (int i = 0; i < count; i++) {
            char coreName[256];
            TextCopy(coreName, lines[i]);
            int len = TextLength(coreName);
            if (len > 0 && coreName[len - 1] == '\r') coreName[len - 1] = '\0';
            if (coreName[0] == '\0') continue;
            AndroidCopyCore(coreName, LIBRETRO.coreDirectory);
        }
        UnloadFileText(list);
    } else {
        TraceLog(LOG_WARNING, "ANDROID: cores.list missing; no bundled cores to install");
    }

    ScanLibretroCoreDirectory();
    if (menu->loadGameWidget != NULL && LIBRETRO.fileBrowserStartDirectory[0] != '\0') {
        nk_console_file_set_directory(menu->loadGameWidget, LIBRETRO.fileBrowserStartDirectory);
    }

    INIT_TRACE("ANDROID/INIT: requesting storage access");
    AndroidRequestStorageAccess(app);
    INIT_TRACE("ANDROID/INIT: enabling immersive mode");
    AndroidEnableImmersiveMode(app);
    INIT_TRACE("ANDROID/INIT: environment setup complete");
}

// android.content.pm.ActivityInfo orientation constants.
#define ANDROID_SCREEN_ORIENTATION_SENSOR_LANDSCAPE  6
#define ANDROID_SCREEN_ORIENTATION_SENSOR_PORTRAIT   7
#define ANDROID_SCREEN_ORIENTATION_SENSOR            4

static void AndroidSetOrientation(struct android_app* app, int index) {
    static const jint orientations[] = {
        ANDROID_SCREEN_ORIENTATION_SENSOR_LANDSCAPE,
        ANDROID_SCREEN_ORIENTATION_SENSOR_PORTRAIT,
        ANDROID_SCREEN_ORIENTATION_SENSOR,
    };
    if (index < 0 || index > 2) index = 0;

    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (env == NULL) return;

    jclass activityClass = (*env)->GetObjectClass(env, app->activity->clazz);
    jmethodID setOrientation = activityClass ? (*env)->GetMethodID(env, activityClass,
        "setRequestedOrientation", "(I)V") : NULL;
    if (setOrientation != NULL) {
        (*env)->CallVoidMethod(env, app->activity->clazz, setOrientation, orientations[index]);
        TraceLog(LOG_INFO, "ANDROID: Set screen orientation (mode %d)", index);
    } else {
        TraceLog(LOG_WARNING, "ANDROID: Could not find setRequestedOrientation method");
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    AndroidJNIEnd(app, needDetach);
}

static bool AndroidGetIntentFilePath(struct android_app* app, char* outPath, int outSize) {
    outPath[0] = '\0';
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (env == NULL) return false;

    bool result = false;

    jclass activityClass = (*env)->GetObjectClass(env, app->activity->clazz);
    jmethodID getIntent = activityClass ? (*env)->GetMethodID(env, activityClass,
        "getIntent", "()Landroid/content/Intent;") : NULL;
    jobject intent = getIntent ?
        (*env)->CallObjectMethod(env, app->activity->clazz, getIntent) : NULL;

    if (intent == NULL) goto done;

    jclass intentClass = (*env)->GetObjectClass(env, intent);
    jmethodID getData = (*env)->GetMethodID(env, intentClass,
        "getData", "()Landroid/net/Uri;");
    jobject uri = getData ? (*env)->CallObjectMethod(env, intent, getData) : NULL;

    if (uri == NULL) {
        TraceLog(LOG_INFO, "ANDROID: Intent has no data URI (normal launch)");
        goto done;
    }

    jclass uriClass = (*env)->GetObjectClass(env, uri);
    jmethodID getScheme = (*env)->GetMethodID(env, uriClass,
        "getScheme", "()Ljava/lang/String;");
    jstring scheme = getScheme ? (jstring)(*env)->CallObjectMethod(env, uri, getScheme) : NULL;
    const char* schemeUtf = scheme ? (*env)->GetStringUTFChars(env, scheme, NULL) : NULL;

    if (schemeUtf != NULL && TextIsEqual(schemeUtf, "file")) {
        jmethodID getPath = (*env)->GetMethodID(env, uriClass,
            "getPath", "()Ljava/lang/String;");
        jstring path = getPath ? (jstring)(*env)->CallObjectMethod(env, uri, getPath) : NULL;
        const char* pathUtf = path ? (*env)->GetStringUTFChars(env, path, NULL) : NULL;
        if (pathUtf != NULL && pathUtf[0] != '\0') {
            TextCopy(outPath, pathUtf);
            result = true;
        }
        if (pathUtf) (*env)->ReleaseStringUTFChars(env, path, pathUtf);
    } else if (schemeUtf != NULL && TextIsEqual(schemeUtf, "content")) {
        const char* dataDir = app->activity->internalDataPath;
        char localPath[RAYLIB_LIBRETRO_VFS_MAX_PATH];

        jmethodID getCR = (*env)->GetMethodID(env, activityClass,
            "getContentResolver", "()Landroid/content/ContentResolver;");
        jobject cr = getCR ? (*env)->CallObjectMethod(env, app->activity->clazz, getCR) : NULL;

        char filename[256] = "";
        if (cr != NULL) {
            jclass crClass = (*env)->GetObjectClass(env, cr);
            jmethodID query = (*env)->GetMethodID(env, crClass, "query",
                "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"
                "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;");
            jclass stringClass = (*env)->FindClass(env, "java/lang/String");
            jobjectArray projection = (*env)->NewObjectArray(env, 1, stringClass, NULL);
            jstring colName = (*env)->NewStringUTF(env, "_display_name");
            (*env)->SetObjectArrayElement(env, projection, 0, colName);
            jobject cursor = query ? (*env)->CallObjectMethod(env, cr, query,
                uri, projection, NULL, NULL, NULL) : NULL;

            if (cursor != NULL) {
                jclass cursorClass = (*env)->GetObjectClass(env, cursor);
                jmethodID moveToFirst = (*env)->GetMethodID(env, cursorClass,
                    "moveToFirst", "()Z");
                jmethodID getString = (*env)->GetMethodID(env, cursorClass,
                    "getString", "(I)Ljava/lang/String;");
                jmethodID close = (*env)->GetMethodID(env, cursorClass,
                    "close", "()V");
                if (moveToFirst && (*env)->CallBooleanMethod(env, cursor, moveToFirst)) {
                    jstring nameStr = getString ?
                        (jstring)(*env)->CallObjectMethod(env, cursor, getString, 0) : NULL;
                    const char* nameUtf = nameStr ?
                        (*env)->GetStringUTFChars(env, nameStr, NULL) : NULL;
                    if (nameUtf != NULL && nameUtf[0] != '\0') {
                        snprintf(filename, sizeof(filename), "%s", nameUtf);
                    }
                    if (nameUtf) (*env)->ReleaseStringUTFChars(env, nameStr, nameUtf);
                }
                if (close) (*env)->CallVoidMethod(env, cursor, close);
            }
        }

        if (filename[0] == '\0') {
            jmethodID getLastSeg = (*env)->GetMethodID(env, uriClass,
                "getLastPathSegment", "()Ljava/lang/String;");
            jstring seg = getLastSeg ?
                (jstring)(*env)->CallObjectMethod(env, uri, getLastSeg) : NULL;
            const char* segUtf = seg ? (*env)->GetStringUTFChars(env, seg, NULL) : NULL;
            if (segUtf && segUtf[0]) snprintf(filename, sizeof(filename), "%s", segUtf);
            if (segUtf) (*env)->ReleaseStringUTFChars(env, seg, segUtf);
        }
        if (filename[0] == '\0') TextCopy(filename, "intent_game");

        snprintf(localPath, sizeof(localPath), "%s/%s", dataDir, filename);

        jmethodID openIS = cr ? (*env)->GetMethodID(env, (*env)->GetObjectClass(env, cr),
            "openInputStream",
            "(Landroid/net/Uri;)Ljava/io/InputStream;") : NULL;
        jobject is = openIS ? (*env)->CallObjectMethod(env, cr, openIS, uri) : NULL;

        if (is != NULL) {
            jclass isClass = (*env)->GetObjectClass(env, is);
            jmethodID readMethod = (*env)->GetMethodID(env, isClass, "read", "([B)I");
            jmethodID closeIS = (*env)->GetMethodID(env, isClass, "close", "()V");

            FILE* fp = fopen(localPath, "wb");
            if (fp != NULL && readMethod) {
                jbyteArray buf = (*env)->NewByteArray(env, 8192);
                jint bytesRead;
                while ((bytesRead = (*env)->CallIntMethod(env, is, readMethod, buf)) > 0) {
                    jbyte* bytes = (*env)->GetByteArrayElements(env, buf, NULL);
                    fwrite(bytes, 1, (size_t)bytesRead, fp);
                    (*env)->ReleaseByteArrayElements(env, buf, bytes, JNI_ABORT);
                }
                fclose(fp);
                TextCopy(outPath, localPath);
                result = true;
                TraceLog(LOG_INFO, "ANDROID: Copied intent file to %s", localPath);
            } else {
                if (fp) fclose(fp);
                TraceLog(LOG_WARNING, "ANDROID: Failed to open %s for writing", localPath);
            }
            if (closeIS) (*env)->CallVoidMethod(env, is, closeIS);
        }
    }

    if (schemeUtf) (*env)->ReleaseStringUTFChars(env, scheme, schemeUtf);

done:
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    AndroidJNIEnd(app, needDetach);
    return result;
}

#endif /* __ANDROID__ */
#endif /* RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION */

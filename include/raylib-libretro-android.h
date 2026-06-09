/**********************************************************************************************
*
*   raylib-libretro-android.h — Android-specific helpers (JNI, file picker, intents).
*
*   Guarded by __ANDROID__; compiles to nothing on other platforms. Define
*   RAYLIB_LIBRETRO_ANDROID_IMPLEMENTATION in exactly one translation unit.
*
*   LICENSE: zlib/libpng
*
**********************************************************************************************/

#ifndef RAYLIB_LIBRETRO_ANDROID_H
#define RAYLIB_LIBRETRO_ANDROID_H

#if defined(__ANDROID__)

#include <jni.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <android/window.h>
#include <android_native_app_glue.h>

extern struct android_app *GetAndroidApp(void);

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

#define ANDROID_FILEPICKER_REQUEST_CODE 42

static char gAndroidFilePickerPath[4096] = {0};
static volatile bool gAndroidFilePickerReady = false;
static pthread_mutex_t gAndroidFilePickerMutex = PTHREAD_MUTEX_INITIALIZER;

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
    if (LIBRETRO.fileBrowserStartDirectory[0] == '\0' && externalDir != NULL) {
        TextCopy(LIBRETRO.fileBrowserStartDirectory, externalDir);
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

static void AndroidSetOrientation(struct android_app* app, int index) {
    static const jint orientations[] = { 6, 7, 4 };
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
    }
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    AndroidJNIEnd(app, needDetach);
}

static bool AndroidCopyContentUri(JNIEnv* env, jobject activity, jobject uri,
                                   const char* destDir, char* outPath, int outPathLen) {
    jclass actClass = (*env)->GetObjectClass(env, activity);
    jmethodID getResolver = actClass ? (*env)->GetMethodID(env, actClass,
        "getContentResolver", "()Landroid/content/ContentResolver;") : NULL;
    jobject resolver = getResolver ? (*env)->CallObjectMethod(env, activity, getResolver) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return false; }
    if (!resolver) return false;

    const char* displayName = "rom";
    jstring jDisplayName = NULL;
    jclass resolverClass = (*env)->GetObjectClass(env, resolver);
    jclass mediaClass = (*env)->FindClass(env, "android/provider/MediaStore$MediaColumns");
    jfieldID dnField = mediaClass ? (*env)->GetStaticFieldID(env, mediaClass,
        "DISPLAY_NAME", "Ljava/lang/String;") : NULL;
    jstring dnCol = dnField ? (jstring)(*env)->GetStaticObjectField(env, mediaClass, dnField) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); dnCol = NULL; }

    if (dnCol) {
        jobjectArray proj = (*env)->NewObjectArray(env, 1,
            (*env)->FindClass(env, "java/lang/String"), dnCol);
        jmethodID query = proj ? (*env)->GetMethodID(env, resolverClass, "query",
            "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;"
            "[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;") : NULL;
        jobject cursor = query ? (*env)->CallObjectMethod(env, resolver, query,
            uri, proj, NULL, NULL, NULL) : NULL;
        if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); cursor = NULL; }

        if (cursor) {
            jclass cursorClass = (*env)->GetObjectClass(env, cursor);
            jmethodID moveFirst = (*env)->GetMethodID(env, cursorClass, "moveToFirst", "()Z");
            jboolean ok = moveFirst ? (*env)->CallBooleanMethod(env, cursor, moveFirst) : JNI_FALSE;
            if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); ok = JNI_FALSE; }

            if (ok) {
                jmethodID getColIdx = (*env)->GetMethodID(env, cursorClass,
                    "getColumnIndex", "(Ljava/lang/String;)I");
                jint idx = getColIdx ? (*env)->CallIntMethod(env, cursor, getColIdx, dnCol) : -1;
                if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); idx = -1; }
                if (idx >= 0) {
                    jmethodID getString = (*env)->GetMethodID(env, cursorClass,
                        "getString", "(I)Ljava/lang/String;");
                    jDisplayName = getString ?
                        (jstring)(*env)->CallObjectMethod(env, cursor, getString, idx) : NULL;
                    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); jDisplayName = NULL; }
                    if (jDisplayName) {
                        displayName = (*env)->GetStringUTFChars(env, jDisplayName, NULL);
                    }
                }
            }
            jmethodID close = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, cursor),
                "close", "()V");
            if (close) (*env)->CallVoidMethod(env, cursor, close);
            if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
        }
    }

    char safeName[256] = {0};
    int j = 0;
    for (int i = 0; displayName[i] && j < (int)sizeof(safeName) - 1; i++) {
        char c = displayName[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_') {
            safeName[j++] = c;
        }
    }
    if (j == 0) { safeName[0] = 'r'; safeName[1] = 'o'; safeName[2] = 'm'; }

    if (jDisplayName && displayName) {
        (*env)->ReleaseStringUTFChars(env, jDisplayName, displayName);
    }

    jmethodID openFd = (*env)->GetMethodID(env, resolverClass, "openFileDescriptor",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");
    jstring modeR = (*env)->NewStringUTF(env, "r");
    jobject pfd = (openFd && modeR) ?
        (*env)->CallObjectMethod(env, resolver, openFd, uri, modeR) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); pfd = NULL; }
    if (!pfd) return false;

    jclass pfdClass = (*env)->GetObjectClass(env, pfd);
    jmethodID detach = (*env)->GetMethodID(env, pfdClass, "detachFd", "()I");
    jint fd = detach ? (*env)->CallIntMethod(env, pfd, detach) : -1;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); fd = -1; }
    if (fd < 0) return false;

    snprintf(outPath, outPathLen, "%s/%s", destDir, safeName);
    FILE* out = fopen(outPath, "wb");
    bool success = false;
    if (out) {
        char buf[65536];
        ssize_t n;
        while ((n = read(fd, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, (size_t)n, out);
        }
        fclose(out);
        success = true;
    }
    close(fd);
    return success;
}

static void AndroidOnActivityResult(ANativeActivity* activity, int32_t requestCode,
                                    int32_t resultCode, void* data) {
    if (requestCode != ANDROID_FILEPICKER_REQUEST_CODE) return;
    if (resultCode != -1 || data == NULL) return;

    struct android_app* app = GetAndroidApp();
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (!env) return;

    jobject intent = (jobject)data;
    jclass intentClass = (*env)->GetObjectClass(env, intent);
    jmethodID getData = intentClass ? (*env)->GetMethodID(env, intentClass, "getData",
        "()Landroid/net/Uri;") : NULL;
    jobject uri = getData ? (*env)->CallObjectMethod(env, intent, getData) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); uri = NULL; }

    if (uri) {
        const char* destDir = app->activity->externalDataPath ?
            app->activity->externalDataPath : app->activity->internalDataPath;
        if (destDir) {
            char tempPath[4096] = {0};
            if (AndroidCopyContentUri(env, activity->clazz, uri, destDir, tempPath, sizeof(tempPath))) {
                pthread_mutex_lock(&gAndroidFilePickerMutex);
                TextCopy(gAndroidFilePickerPath, tempPath);
                gAndroidFilePickerReady = true;
                pthread_mutex_unlock(&gAndroidFilePickerMutex);
                TraceLog(LOG_INFO, "ANDROID: File picker copied to %s", tempPath);
            } else {
                TraceLog(LOG_WARNING, "ANDROID: Failed to copy file picker selection");
            }
        }
    }

    AndroidJNIEnd(app, needDetach);
}

static void LibretroMenuAndroidLoadGameClicked(nk_console* widget, void* user_data) {
    (void)widget;
    (void)user_data;

    struct android_app* app = GetAndroidApp();
    bool needDetach = false;
    JNIEnv* env = AndroidJNIBegin(app, &needDetach);
    if (!env) {
        TraceLog(LOG_WARNING, "ANDROID: Could not attach JNI for file picker");
        return;
    }

    app->activity->callbacks->onActivityResult = AndroidOnActivityResult;

    jclass intentClass = (*env)->FindClass(env, "android/content/Intent");
    jfieldID actionField = intentClass ? (*env)->GetStaticFieldID(env, intentClass,
        "ACTION_OPEN_DOCUMENT", "Ljava/lang/String;") : NULL;
    jstring action = actionField ?
        (jstring)(*env)->GetStaticObjectField(env, intentClass, actionField) : NULL;
    jmethodID initStr = intentClass ? (*env)->GetMethodID(env, intentClass, "<init>",
        "(Ljava/lang/String;)V") : NULL;
    jobject intent = (initStr && action) ?
        (*env)->NewObject(env, intentClass, initStr, action) : NULL;
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); intent = NULL; }

    if (intent) {
        jfieldID catField = (*env)->GetStaticFieldID(env, intentClass,
            "CATEGORY_OPENABLE", "Ljava/lang/String;");
        jstring category = catField ?
            (jstring)(*env)->GetStaticObjectField(env, intentClass, catField) : NULL;
        jmethodID addCat = (*env)->GetMethodID(env, intentClass, "addCategory",
            "(Ljava/lang/String;)Landroid/content/Intent;");
        if (category && addCat) (*env)->CallObjectMethod(env, intent, addCat, category);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

        jmethodID setType = (*env)->GetMethodID(env, intentClass, "setType",
            "(Ljava/lang/String;)Landroid/content/Intent;");
        jstring mimeAll = (*env)->NewStringUTF(env, "*/*");
        if (setType && mimeAll) (*env)->CallObjectMethod(env, intent, setType, mimeAll);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

        jclass actClass = (*env)->GetObjectClass(env, app->activity->clazz);
        jmethodID startForResult = actClass ? (*env)->GetMethodID(env, actClass,
            "startActivityForResult", "(Landroid/content/Intent;I)V") : NULL;
        if (startForResult) {
            (*env)->CallVoidMethod(env, app->activity->clazz, startForResult,
                intent, (jint)ANDROID_FILEPICKER_REQUEST_CODE);
            TraceLog(LOG_INFO, "ANDROID: Launched native file picker");
        }
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }

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

    if (uri == NULL) goto done;

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

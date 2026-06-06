# raylib-libretro for Android :space_invader:

The Android port of [raylib-libretro](../README.md). It builds the full frontend
([bin/raylib-libretro.c](../bin/raylib-libretro.c)) into a native APK that runs as
an `android.app.NativeActivity` — there is no Java/Kotlin application code
(`android:hasCode="false"`).

## Requirements

- **Android SDK** with `platforms;android-34` and the matching build-tools
- **Android NDK** `27.3.13750724` (set in [app/build.gradle](app/build.gradle))
- **CMake** 3.22.1 (installed via the SDK manager; invoked through Gradle's `externalNativeBuild`)
- **JDK 17+** (required by Android Gradle Plugin 8.2)
- A `local.properties` (or `$ANDROID_HOME`) pointing at your SDK:
  ```properties
  sdk.dir=/path/to/Android/Sdk
  ```
  This file is machine-specific and is git-ignored — do not commit it.

| Setting        | Value                |
|----------------|----------------------|
| applicationId  | `com.raylib.libretro`|
| compileSdk     | 34                   |
| minSdk         | 24 (Android 7.0)     |
| targetSdk      | 34                   |
| ABI            | `arm64-v8a` only     |
| STL            | `c++_shared`         |
| AGP / Gradle   | 8.2.2 / 8.6          |

> **Only `arm64-v8a` is built.** The APK installs on physical arm64 devices but
> **not** on an x86_64 emulator. To test on an emulator, add `x86_64` to
> `abiFilters` in [app/build.gradle](app/build.gradle).

## Build

The Gradle wrapper is committed (pinned to Gradle 8.6), so `./gradlew` works
without a separate Gradle install:

```sh
cd android
git -C .. submodule update --init   # raylib, libretro-common, physfs, etc.

./gradlew assembleDebug
```

The signed-with-debug-key APK lands at:

```
android/app/build/outputs/apk/debug/app-debug.apk
```

> The first configure downloads the bundled libretro cores from the libretro
> nightly buildbot into `app/src/main/assets/cores/` (see
> [app/CMakeLists.txt](app/CMakeLists.txt)). This needs network access and is
> cached for subsequent builds.

## Releases

Prebuilt APKs are attached to every
[GitHub Release](https://github.com/RobLoach/raylib-libretro/releases), built by
[.github/workflows/Release.yml](../.github/workflows/Release.yml) when a release
is published. The asset is named `raylib-libretro-android-arm64.apk`; its
`versionName` is taken from the release tag and `versionCode` from the build
number, so each release is a valid update over the last.

> **Known limitation:** release APKs are signed with the Android **debug key**,
> which is not stable across builds. Installing a newer release over an existing
> install fails with a signature mismatch — uninstall the previous version first
> (this loses save data). A persistent release-signing key would remove this.

## Install & run

```sh
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

On first launch the app:

1. Copies the bundled cores out of the read-only APK assets into its writable
   data directory and loads them from there (Android can only `dlopen()` a core
   from a real filesystem path).
2. Requests **All-Files Access** so the in-app file browser can reach ROMs on
   shared storage. Grant it on the settings screen that appears, then return to
   the app.

Then open the menu, pick a core (or let it autodetect from the ROM), and load a
game.

### Where to put ROMs

With All-Files Access granted you can browse the whole device. The file browser
starts in the app's external files directory, which needs no special permission:

```sh
adb push smb.nes /sdcard/Android/data/com.raylib.libretro/files/
```

## How it works

### Native entry point
`NativeActivity` loads `libmain.so` and looks up `ANativeActivity_onCreate` at
runtime. That symbol comes from raylib's bundled `android_native_app_glue.c`, so
[app/CMakeLists.txt](app/CMakeLists.txt) passes `-u ANativeActivity_onCreate`
when linking `libmain` to force it to be kept and exported. Without it the APK
crashes instantly with a blank screen.

### Cores
The bundled cores — **fceumm, snes9x, mgba, picodrive** — are
downloaded at configure time into `app/src/main/assets/cores/`, with a generated
`cores.list` naming them. They are stored uncompressed (`noCompress 'so'`) so
they can be read and copied out efficiently. `SetupAndroidEnvironment()` in
[../bin/raylib-libretro.c](../bin/raylib-libretro.c) installs them into the data
directory on first launch.

To change the core set, edit the URL list in [app/CMakeLists.txt](app/CMakeLists.txt)
and rebuild — `cores.list` regenerates automatically.

### Directories
The desktop defaults (`cores`, `saves`, `system`) are relative and unusable on
Android, where the working directory is `/`. At startup they are rewritten to
absolute paths under the app's `internalDataPath`. Values you change later in
the in-app Settings persist and are not overwritten.

### App icon
The launcher icon is defined as an **adaptive icon** with vector layers:

- [app/src/main/res/drawable/ic_launcher_foreground.xml](app/src/main/res/drawable/ic_launcher_foreground.xml) — the pixel invader as a vector
- [app/src/main/res/drawable/ic_launcher_background.xml](app/src/main/res/drawable/ic_launcher_background.xml) — a dark gradient
- a `<monochrome>` layer for Android 13+ Material You themed icons

Raster `ic_launcher.png` / `ic_launcher_round.png` in the `mipmap-*dpi` folders
are fallbacks for API 24–25, which predate adaptive icons. They are generated
from [../bin/icon-512.png](../bin/icon-512.png).

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| `./gradlew: Permission denied` | The wrapper lost its executable bit — `chmod +x gradlew`. |
| Crashes instantly, blank screen | Missing `ANativeActivity_onCreate` — verify the `-u` link flag survived (`nm -D libmain.so \| grep ANativeActivity_onCreate`). |
| Installs but won't launch on emulator | Emulator is x86_64; only `arm64-v8a` is built. Add `x86_64` to `abiFilters`. |
| Won't install over an older build (signature mismatch) | Release APKs are debug-signed; uninstall the previous version first. |
| Menu shows no cores | The core download failed at build time (logged as a CMake warning), or first-launch copy failed — check `adb logcat`. |
| Can't find ROMs in the browser | All-Files Access not granted, or ROMs are outside the app's storage. |

Watch the native logs during launch:

```sh
adb logcat | grep -iE "raylib|libretro|ANativeActivity|native"
```

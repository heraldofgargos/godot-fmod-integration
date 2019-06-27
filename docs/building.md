## Building the module

To compile the module for a different version of Godot, follow these instructions. Precompiled engine binaries are available in the Releases tab but it is recommended to perform a recompilation of the engine with the version of Godot that is currently being used in your project.

1. [Download the FMOD Studio API](https://www.fmod.com/download) (You need to create an account) and extract it somewhere on your system. This integration currently uses the 2.00.02 Early Access release.
2. Clone the version of Godot currently being used in your project or simply clone the latest version from the [master branch](https://github.com/godotengine/godot).
3. `cd` into the source directory and add the FMOD integration as a submodule into the `modules` directory `git submodule add https://github.com/alexfonseka/godot-fmod-integration modules/fmod`.
4. Copy the contents of the `api` directory of the FMOD API into the module's `api` directory `modules/fmod/api`. On Windows this is (usually) found at `C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api`.
5. Recompile the engine. For more information on compiling the engine, refer to the [Godot documentation](https://docs.godotengine.org/en/latest/development/compiling/index.html).
6. Place the FMOD dynamically linking library files within the `bin` directory for Godot to start. Eg. on Windows these would be `fmod.dll` and `fmodstudio.dll`. When shipping, these files have to be included with the release.

### Godot FMOD GDNative

Alternatively, the GDNative version of the integration is being developed [here](https://github.com/utopia-rise/fmod-gdnative). This allows the use of FMOD Studio without an engine recompilation 👍.

### For Android build targets

Before building the engine, you should first create the environment variable to get the NDK path.

`export ANDROID_NDK_ROOT=pathToYourNDK`

In order to get FMOD working on Android, you need to make Fmod java static initialization in Godot's Android export
template. To do so, follow the next steps.

- Add fmod.jar as dependency in your project.
  In order to add FMOD to Gradle you should have dependencies looking like this :

```
dependencies {
	implementation "com.android.support:support-core-utils:28.0.0"
	compile files("libs/fmod.jar")
}
```

- Modify `onCreate` and `onDestroy` methods in `Godot` Java class

For `onCreate` you should initialize Java part of FMOD.

```java
	@Override
	protected void onCreate(Bundle icicle) {

		super.onCreate(icicle);
		FMOD.init(this);
		Window window = getWindow();
		...
	}
```

For `onDestroy` method, you should close Java part of FMOD.

```java
	@Override
	protected void onDestroy() {

		if (mPaymentsManager != null) mPaymentsManager.destroy();
		for (int i = 0; i < singleton_count; i++) {
			singletons[i].onMainDestroy();
		}
		FMOD.close();
		super.onDestroy();
	}
```

- Then run `./gradlew build` to generate an apk export template. You can then use it in your project to get FMOD working
  on Android.

# FMOD Studio integration for Godot

A Godot C++ module that provides an integration and GDScript bindings for the FMOD Studio API.

FMOD is an audio engine and middleware solution for interactive audio in games. It has been the audio engine behind many titles such as Transistor, Into the Breach and Celeste. [More on FMOD's website](https://www.fmod.com/).

This module exposes most of the Studio API functions to Godot's GDScript and also provides helpers for performing common functions like attaching Studio events to Godot nodes and playing 3D/positional audio. _It is still very much a work in progress and some API functions are not yet exposed._ Feel free to tweak/extend it based on your project's needs.

**Note:** FMOD also provides a C# wrapper for their API which is used in the Unity integration and it is possible to use the same wrapper to build an integration for Godot in C#. However do note that this would only work on a Mono build of Godot and performance might not be on the same level as a Native/C++ integration.

## Installing the module

1. [Download the FMOD Studio API](https://www.fmod.com/download) (You need to create an account) and install it on your system. This integration currently uses the 2.00.00 Early Access release.
2. Clone the latest version of Godot from the [master branch](https://github.com/godotengine/godot). At the time of writing this is Godot 3.1 beta.
3. `cd` into the source directory and add the FMOD integration as a submodule into the `modules` directory `git submodule add https://github.com/alexfonseka/godot-fmod-integration modules/fmod`.
4. Copy the contents of the `api` directory of the FMOD API into the module's `api` directory `modules/fmod/api`. On Windows this is (usually) found at `C:/Program Files (x86)/FMOD SoundSystem/FMOD Studio API Windows/api`.
5. Recompile the engine. For more information on compiling the engine, refer to the [Godot documentation](https://docs.godotengine.org/en/latest/development/compiling/index.html).
6. Place the FMOD library files within the `bin` directory for Godot to start. Eg. on Windows these would be `fmod.dll` and `fmodstudio.dll`. When shipping, these files have to be included with the release.

## Using the module

### Basic usage

```gdscript
extends Node

# create an instance of the module
# ideally this has to be done in an AutoLoad script
# as that way you'll be able to call FMOD functions from any script
# refer to the demo project provided
var FMOD = Fmod.new()

func _ready():
	# set up FMOD
	FMOD.system_set_software_format(0, Fmod.FMOD_SPEAKERMODE_STEREO, 0)
	# initializing with the LIVE_UPDATE flag lets you
	# connect to Godot from the FMOD Studio editor
	# and author events in realtime
	FMOD.system_init(1024, Fmod.FMOD_STUDIO_INIT_LIVEUPDATE, Fmod.FMOD_INIT_NORMAL)

	# load banks
	# place your banks inside the project directory
	FMOD.bank_load("./Banks/Desktop/Master Bank.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)
	FMOD.bank_load("./Banks/Desktop/Master Bank.strings.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)

	# register listener
	FMOD.system_add_listener($Listener)

	# play some events
	FMOD.play_one_shot("event:/Footstep", $SoundSource1)
	FMOD.play_one_shot("event:/Gunshot", $SoundSource2)

func _process(delta):
	# update FMOD every tick
	# calling system_update also updates the listener 3D position
	# and 3D positions of any attached event instances
	FMOD.system_update()
```

### Calling Studio events

One-shots are great for quick sounds which you would want to simply fire and forget. But what about something a bit more complex like a looping sound or an interactive music event with a bunch of states? Here's an example of a Studio event called manually (ie. not directly managed by the integration). You can then call functions on that specific instance such as setting parameters. Remember to release the instance once you're done with it!

```gdscript
# create an event instance
# this is a music event that has been authored in the Studio editor
var my_music_event = FMOD.event_create_instance("event:/Waveshaper - Wisdom of Rage")

# start the event
FMOD.event_start(my_music_event)

# wait a bit
yield(music_state_timer, "timeout")

# setting an event parameter
# in this case causes the music to transition to the next phase
FMOD.event_set_parameter(my_music_event, "State", 2.0)

# wait a bit
yield(music_timer, "timeout")

# stop the event
FMOD.event_stop(my_music_event, Fmod.FMOD_STUDIO_STOP_ALLOWFADEOUT)

# release the event
FMOD.event_release(my_music_event)
```

### Using the integration helpers

These are helper functions provided by the integration for playing events and attaching event instances to Godot Nodes for 3D/positional audio. The listener position and 3D attributes of any attached instances are automatically updated every time you call `system_update()`. Instances are also automatically cleaned up once finished so you don't have to manually call `event_release()`.

```gdscript
# play an event at this Node's position
# 3D attributes are only set ONCE
# parameters cannot be set
FMOD.play_one_shot("event:/Footstep", self)

# same as play_one_shot but lets you set initial parameters
# subsequent parameters cannot be set
FMOD.play_one_shot_with_params("event:/Footstep", self, { "Surface": 1.0, "Speed": 2.0 })

# play an event attached to this Node
# 3D attributes are automatically set every frame (when update is called)
# parameters cannot be set
FMOD.play_one_shot_attached("event:/Footstep", self)

# same as play_one_shot_attached but lets you set initial parameters
# subsequent parameters cannot be set
FMOD.play_one_shot_attached_with_params("event:/Footstep", self, { "Surface": 1.0, "Speed": 2.0 })

# attaches a manually called instance to a Node
# once attached 3D attributes are automatically set every frame (when update is called)
FMOD.attach_instance_to_node(event_instance, self)

# detaches the instance from its Node
FMOD.detach_instance_from_node(event_instance)

# quick helpers for pausing and muting
# affects all events including manually called instances
FMOD.pause_all_events()
FMOD.unpause_all_events()
FMOD.mute_all_events()
FMOD.unmute_all_events()

# returns True if a bank is currently loading
FMOD.banks_still_loading()
```

### Timeline marker & music beat callbacks

You can have events subscribe to Studio callbacks to implement rhythm based game mechanics. Event callbacks leverage Godot's signal system and you can connect your callback functions through the integration.

```gdscript
# create a new event instance
var my_music_event = FMOD.event_create_instance("event:/schmid - 140 Part 2B")

# request callbacks from this instance
# in this case request both Marker and Beat callbacks
FMOD.event_set_callback(my_music_event,
	Fmod.FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER | Fmod.FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT)

# hook up our signals
FMOD.connect("timeline_beat", self, "_on_beat")
FMOD.connect("timeline_marker", self, "_on_marker")

# will be called on every musical beat
func _on_beat(params):
	print(params)

# will be called whenever a new marker is encountered
func _on_marker(params):
	print(params)
```

In the above example, `params` is a Dictionary which contains parameters passed in by FMOD. These vary from each callback. For beat callbacks it will contain fields such as the current beat, current bar, time signature etc. For marker callbacks it will contain the marker name etc. The event_id of the instance that triggered the callback will also be passed in. You can use this to filter out individual callbacks if multiple events are subscribed.

### Playing sounds using FMOD Core / Low Level API

You can load and play any sound file in your project directory using the FMOD Low Level API bindings. Similar to Studio events these instances have to be released manually. Refer to FMOD's documentation pages for a list of compatible sound formats. If you're using FMOD Studio it's unlikely you'll have to use this API though.

```gdscript
# create a sound
var my_sound = FMOD.sound_load("./ta-da.wav", Fmod.FMOD_DEFAULT)

FMOD.sound_play(my_sound)

# wait a bit
yield(sound_timer, "timeout")

FMOD.sound_stop(my_sound)
FMOD.sound_release(my_sound)
```

### Changing the default audio output device

By default, FMOD will use the primary audio output device as determined by the operating system. This can be changed at runtime, ideally through your game's Options Menu.

Here, `system_get_available_drivers()` returns an Array which contains a Dictionary for every audio driver found. Each Dictionary contains fields such as the name, sample rate
and speaker config of the respective driver. Most importantly, it contains the id for that driver.

```python
# retrieve all available audio drivers
var drivers = FMOD.system_get_available_drivers()

# change the audio driver
# you must pass in the id of the respective driver
FMOD.system_set_driver(id)

# retrieve the id of the currently set driver
var id = FMOD.system_get_driver()
```

## For Android build targets

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

## Contributing

This project is still a work in progress and is probably not yet ready for use in full-blown production. If you run into issues (crashes, memory leaks, broken 3D sound etc.) let us know through the [issue tracker](https://github.com/alexfonseka/godot-fmod-integration/issues). If you are a programmer, sound designer or a composer and wish to contribute, the contribution guidelines are available [here](https://github.com/alexfonseka/godot-fmod-integration/blob/master/.github/contributing.md). Thank you for being interested in this project! ✌

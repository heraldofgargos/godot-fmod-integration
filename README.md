# FMOD Studio integration for Godot

A Godot C++ module that provides an integration and GDScript bindings for the FMOD Studio API.

FMOD is an audio engine and middleware solution for interactive audio in games. It has been the audio engine behind many titles such as Transistor, Into the Breach and Celeste. [More on FMOD's website](https://www.fmod.com/).

This module exposes most of the Studio API functions to Godot's GDScript and also provides helpers for performing common functions like attaching Studio events to Godot nodes and playing 3D/positional audio. _It is still very much a work in progress and some API functions are not yet exposed._ Feel free to tweak/extend it based on your project's needs.

### Latest release

Precompiled engine binaries for Windows, macOS and Linux with FMOD Studio already integrated, is available for downloading in the [Releases](https://github.com/alexfonseka/godot-fmod-integration/releases) tab.

| Current build status | [![Build Status](https://travis-ci.com/alexfonseka/godot-fmod-integration.svg?branch=master)](https://travis-ci.com/alexfonseka/godot-fmod-integration) |
| -------------------- | :-----------------------------------------------------------------------------------------------------------------------------------------------------: |


### Building

If you wish to compile the module yourself, build instructions are available [here](https://github.com/alexfonseka/godot-fmod-integration/blob/master/docs/building.md).

## Using the module

### Basic usage

Start playing sounds in just 5 lines of GDScript!

```gdscript
extends Node

func _ready():
	# initialize FMOD
	# initializing with the LIVE_UPDATE flag lets you
	# connect to Godot from the FMOD Studio editor
	# and author events in realtime
	Fmod.system_init(1024, Fmod.FMOD_STUDIO_INIT_LIVEUPDATE, Fmod.FMOD_INIT_NORMAL)

	# load banks
	# place your banks inside the project directory
	Fmod.bank_load("./Banks/Desktop/Master Bank.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)
	Fmod.bank_load("./Banks/Desktop/Master Bank.strings.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)

	# register a listener
	Fmod.system_add_listener($Listener)

	# play some events
	Fmod.play_one_shot("event:/Footstep", $SoundSource1)
	Fmod.play_one_shot("event:/Gunshot", $SoundSource2)

func _process(delta):
	# update FMOD every tick
	# calling system_update also updates the listener 3D position
	# and 3D positions of any attached event instances
	Fmod.system_update()
```

### Calling Studio events

One-shots are great for quick sounds which you would want to simply fire and forget. But what about something a bit more complex like a looping sound or an interactive music event with a bunch of states? Here's an example of a Studio event called manually (ie. not directly managed by the integration). You can then call functions on that specific instance such as setting parameters. Remember to release the instance once you're done with it!

```gdscript
# create an event instance
# this is a music event that has been authored in the Studio editor
var my_music_event = Fmod.create_event_instance("event:/Waveshaper - Wisdom of Rage")

# start the event
Fmod.event_start(my_music_event)

# wait a bit
yield(music_state_timer, "timeout")

# setting an event parameter
# in this case causes the music to transition to the next phase
Fmod.event_set_parameter(my_music_event, "State", 2.0)

# wait a bit
yield(music_timer, "timeout")

# stop the event
Fmod.event_stop(my_music_event, Fmod.FMOD_STUDIO_STOP_ALLOWFADEOUT)

# release the event
Fmod.event_release(my_music_event)
```

### Using the integration helpers

These are helper functions provided by the integration for playing events and attaching event instances to Godot Nodes for 3D/positional audio. The listener position and 3D attributes of any attached instances are automatically updated every time you call `system_update()`. Instances are also automatically cleaned up once finished so you don't have to manually call `event_release()`.

```gdscript
# play an event at this Node's position
# 3D attributes are only set ONCE
# parameters cannot be set
Fmod.play_one_shot("event:/Footstep", self)

# same as play_one_shot but lets you set initial parameters
# subsequent parameters cannot be set
Fmod.play_one_shot_with_params("event:/Footstep", self, { "Surface": 1.0, "Speed": 2.0 })

# play an event attached to this Node
# 3D attributes are automatically set every frame (when update is called)
# parameters cannot be set
Fmod.play_one_shot_attached("event:/Footstep", self)

# same as play_one_shot_attached but lets you set initial parameters
# subsequent parameters cannot be set
Fmod.play_one_shot_attached_with_params("event:/Footstep", self, { "Surface": 1.0, "Speed": 2.0 })

# attaches a manually called instance to a Node
# once attached 3D attributes are automatically set every frame (when update is called)
Fmod.attach_instance_to_node(event_instance, self)

# detaches the instance from its Node
Fmod.detach_instance_from_node(event_instance)

# quick helpers for pausing and muting
# affects all events including manually called instances
Fmod.pause_all_events()
Fmod.unpause_all_events()
Fmod.mute_all_events()
Fmod.unmute_all_events()

# returns True if a bank is currently loading
Fmod.banks_still_loading()

# blocks the calling thread until all sample loading is done
Fmod.wait_for_all_loads()
```

### Timeline marker & music beat callbacks

You can have events subscribe to Studio callbacks to implement rhythm based game mechanics. Event callbacks leverage Godot's signal system and you can connect your callback functions through the integration.

```gdscript
# create a new event instance
var my_music_event = Fmod.create_event_instance("event:/schmid - 140 Part 2B")

# request callbacks from this instance
# in this case request both Marker and Beat callbacks
Fmod.event_set_callback(my_music_event,
	Fmod.FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER | Fmod.FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT)

# hook up our signals
Fmod.connect("timeline_beat", self, "_on_beat")
Fmod.connect("timeline_marker", self, "_on_marker")

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
var my_sound = Fmod.sound_load("./ta-da.wav", Fmod.FMOD_DEFAULT)

Fmod.sound_play(my_sound)

# wait a bit
yield(sound_timer, "timeout")

Fmod.sound_stop(my_sound)
Fmod.sound_release(my_sound)
```

### Changing the default audio output device

By default, FMOD will use the primary audio output device as determined by the operating system. This can be changed at runtime, ideally through your game's Options Menu.

Here, `system_get_available_drivers()` returns an Array which contains a Dictionary for every audio driver found. Each Dictionary contains fields such as the name, sample rate
and speaker config of the respective driver. Most importantly, it contains the id for that driver.

```gdscript
# retrieve all available audio drivers
var drivers = Fmod.system_get_available_drivers()

# change the audio driver
# you must pass in the id of the respective driver
Fmod.system_set_driver(id)

# retrieve the id of the currently set driver
var id = Fmod.system_get_driver()
```

### Profiling & querying performance data

`system_get_performance_data()` returns an object which contains current performance stats for CPU, Memory and File Streaming usage of both FMOD Studio and the Core System.

```gdscript
# called every frame
var perf_data = Fmod.system_get_performance_data()

print(perf_data.CPU)
print(perf_data.memory)
print(perf_data.file)
```

## Contributing

This project is still a work in progress and is probably not yet ready for use in full-blown production. If you run into issues (crashes, memory leaks, broken 3D sound etc.) let us know through the [issue tracker](https://github.com/alexfonseka/godot-fmod-integration/issues). If you are a programmer, sound designer or a composer and wish to contribute, the contribution guidelines are available [here](https://github.com/alexfonseka/godot-fmod-integration/blob/master/.github/contributing.md). Thank you for being interested in this project! ✌

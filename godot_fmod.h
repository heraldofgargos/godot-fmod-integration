/*************************************************************************/
/*  godot_fmod.h                                                         */
/*************************************************************************/
/*                                                                       */
/*       FMOD Studio module and bindings for the Godot game engine       */
/*                                                                       */
/*************************************************************************/
/* Copyright (c) 2019 Alex Fonseka                                       */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#pragma once

#include "core/array.h"
#include "core/dictionary.h"
#include "core/map.h"
#include "core/node_path.h"
#include "core/object.h"
#include "core/reference.h"
#include "core/vector.h"
#include "scene/2d/canvas_item.h"
#include "scene/3d/spatial.h"
#include "scene/main/node.h"

#include "api/core/inc/fmod.hpp"
#include "api/core/inc/fmod_errors.h"
#include "api/studio/inc/fmod_studio.hpp"
#include "callbacks.h"

class Fmod : public Object
{
public:
	struct EventInfo
	{
		// Is this event a one-shot instance managed by the integration
		bool isOneShot = false;

		// GameObject to which this event is attached
		Object *gameObj = nullptr;

		// Callback info associated with this event
		Callbacks::CallbackInfo callbackInfo = Callbacks::CallbackInfo();

		bool isMuted = false;

		// Keep track of the event's previous volume level if muted
		float oldVolume = 0.f;
	};

private:
	GDCLASS(Fmod, Object);

	FMOD::Studio::System *system;
	FMOD::System *coreSystem;

	Object *listener;

	bool nullListenerWarning = true;

	float distanceScale = 1.0f;

	Map<String, FMOD::Studio::Bank *> banks;
	Map<String, FMOD::Studio::EventDescription *> eventDescriptions;
	Map<uint64_t, FMOD::Studio::EventDescription *> ptrToEventDescMap;
	Map<String, FMOD::Studio::Bus *> buses;
	Map<String, FMOD::Studio::VCA *> VCAs;

	Map<uint64_t, FMOD::Studio::EventInstance *> events;

	// for playing sounds using FMOD Core / Low Level
	Map<uint64_t, FMOD::Sound *> sounds;
	Map<FMOD::Sound *, FMOD::Channel *> channels;

	FMOD_3D_ATTRIBUTES get3DAttributes(FMOD_VECTOR pos, FMOD_VECTOR up, FMOD_VECTOR forward, FMOD_VECTOR vel);
	FMOD_VECTOR toFmodVector(Vector3 vec);
	void setListenerAttributes();
	void updateInstance3DAttributes(FMOD::Studio::EventInstance *i, Object *o);
	int checkErrors(FMOD_RESULT result);
	bool isNull(Object *o);
	void loadBus(const String &busPath);
	void loadVCA(const String &VCAPath);
	void runCallbacks();
	FMOD::Studio::EventInstance *createInstance(String eventPath, bool isOneShot, bool isAttached, Object *gameObject);
	EventInfo *getEventInfo(FMOD::Studio::EventInstance *eventInstance);
	void releaseOneEvent(FMOD::Studio::EventInstance *eventInstance);

protected:
	static Fmod *singleton;
	static void _bind_methods();

public:
	/* System functions */
	void init(int numOfChannels, int studioFlags, int flags);
	void update();
	void shutdown();
	void addListener(Object *gameObj);
	void setSoftwareFormat(int sampleRate, int speakerMode, int numRawSpeakers);
	void setSound3DSettings(float dopplerScale, float distanceFactor, float rollOffScale);
	uint64_t getEvent(const String &path);
	void setGlobalParameter(const String &parameterName, float value);
	float getGlobalParameter(const String &parameterName);
	Array getAvailableDrivers();
	int getDriver();
	void setDriver(int id);
	Dictionary getPerformanceData();

	/* Helper functions */
	uint64_t createEventInstance(const String &eventPath);
	void playOneShot(const String &eventName, Object *gameObj);
	void playOneShotWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters);
	void playOneShotAttached(const String &eventName, Object *gameObj);
	void playOneShotAttachedWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters);
	void attachInstanceToNode(uint64_t instanceId, Object *gameObj);
	void detachInstanceFromNode(uint64_t instanceId);
	void pauseAllEvents();
	void unpauseAllEvents();
	void muteMasterBus();
	void unmuteMasterBus();
	bool banksStillLoading();
	void waitForAllLoads();

	/* Bank functions */
	String loadbank(const String &pathToBank, int flags);
	void unloadBank(const String &pathToBank);
	int getBankLoadingState(const String &pathToBank);
	int getBankBusCount(const String &pathToBank);
	int getBankEventCount(const String &pathToBank);
	int getBankStringCount(const String &pathToBank);
	int getBankVCACount(const String &pathToBank);

	/* EventDescription functions */
	uint64_t descCreateInstance(uint64_t descHandle);
	int descGetLength(uint64_t descHandle);
	String descGetPath(uint64_t descHandle);
	Array descGetInstanceList(uint64_t descHandle);
	int descGetInstanceCount(uint64_t descHandle);
	void descReleaseAllInstances(uint64_t descHandle);
	void descLoadSampleData(uint64_t descHandle);
	void descUnloadSampleData(uint64_t descHandle);
	int descGetSampleLoadingState(uint64_t descHandle);
	bool descIs3D(uint64_t descHandle);
	bool descIsOneShot(uint64_t descHandle);
	bool descIsSnapshot(uint64_t descHandle);
	bool descIsStream(uint64_t descHandle);
	bool descHasCue(uint64_t descHandle);
	float descGetMaximumDistance(uint64_t descHandle);
	float descGetMinimumDistance(uint64_t descHandle);
	float descGetSoundSize(uint64_t descHandle);
	Dictionary descGetParameterDescriptionByName(uint64_t descHandle, const String &name);
	Dictionary descGetParameterDescriptionByID(uint64_t descHandle, Array idPair);
	int descGetParameterDescriptionCount(uint64_t descHandle);
	Dictionary descGetParameterDescriptionByIndex(uint64_t descHandle, int index);
	Dictionary descGetUserProperty(uint64_t descHandle, String name);
	int descGetUserPropertyCount(uint64_t descHandle);
	Dictionary descUserPropertyByIndex(uint64_t descHandle, int index);

	/* EventInstance functions */
	float getEventParameter(uint64_t instanceId, const String &parameterName);
	void setEventParameter(uint64_t instanceId, const String &parameterName, float value);
	void releaseEvent(uint64_t instanceId);
	void startEvent(uint64_t instanceId);
	void stopEvent(uint64_t instanceId, int stopMode);
	void triggerEventCue(uint64_t instanceId);
	int getEventPlaybackState(uint64_t instanceId);
	bool getEventPaused(uint64_t instanceId);
	void setEventPaused(uint64_t instanceId, bool paused);
	float getEventPitch(uint64_t instanceId);
	void setEventPitch(uint64_t instanceId, float pitch);
	float getEventVolume(uint64_t instanceId);
	void setEventVolume(uint64_t instanceId, float volume);
	int getEventTimelinePosition(uint64_t instanceId);
	void setEventTimelinePosition(uint64_t instanceId, int position);
	float getEventReverbLevel(uint64_t instanceId, int index);
	void setEventReverbLevel(uint64_t instanceId, int index, float level);
	bool isEventVirtual(uint64_t instanceId);
	void setCallback(uint64_t instanceId, int callbackMask);
	uint64_t getEventDescription(uint64_t instanceId);

	/* Bus functions */
	bool getBusMute(const String &busPath);
	bool getBusPaused(const String &busPath);
	float getBusVolume(const String &busPath);
	void setBusMute(const String &busPath, bool mute);
	void setBusPaused(const String &busPath, bool paused);
	void setBusVolume(const String &busPath, float volume);
	void stopAllBusEvents(const String &busPath, int stopMode);

	/* VCA functions */
	float getVCAVolume(const String &VCAPath);
	void setVCAVolume(const String &VCAPath, float volume);

	/* FMOD Core Sound functions */
	void playSound(uint64_t instanceId);
	uint64_t loadSound(const String &path, int mode);
	void releaseSound(uint64_t instanceId);
	void setSoundPaused(uint64_t instanceId, bool paused);
	void stopSound(uint64_t instanceId);
	bool isSoundPlaying(uint64_t instanceId);
	void setSoundVolume(uint64_t instanceId, float volume);
	float getSoundVolume(uint64_t instanceId);
	float getSoundPitch(uint64_t instanceId);
	void setSoundPitch(uint64_t instanceId, float pitch);

	static Fmod *getSingleton();

	Fmod();
	~Fmod();
};
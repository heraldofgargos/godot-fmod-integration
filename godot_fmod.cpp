/*************************************************************************/
/*  godot_fmod.cpp                                                       */
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

#include "godot_fmod.h"
#include "callbacks.h"

void Fmod::init(int numOfChannels, int studioFlags, int flags) {
	// initialize FMOD Studio and FMOD Low Level System with provided flags
	if (checkErrors(system->initialize(numOfChannels, studioFlags, flags, nullptr))) {
		print_line("FMOD Sound System: Successfully initialized");
		if (studioFlags == FMOD_STUDIO_INIT_LIVEUPDATE)
			print_line("FMOD Sound System: Live update enabled!");
	} else
		print_error("FMOD Sound System: Failed to initialize :|");
}

void Fmod::update() {
	// clean up one shots
	for (int i = 0; i < oneShotInstances.size(); i++) {
		auto instance = oneShotInstances.get(i);
		FMOD_STUDIO_PLAYBACK_STATE s;
		checkErrors(instance->getPlaybackState(&s));
		if (s == FMOD_STUDIO_PLAYBACK_STOPPED) {
			checkErrors(instance->release());
			oneShotInstances.remove(i);
			i--;
		}
	}

	// update and clean up attached one shots
	for (int i = 0; i < attachedOneShots.size(); i++) {
		auto aShot = attachedOneShots.get(i);
		if (isNull(aShot.gameObj)) {
			FMOD_STUDIO_STOP_MODE m = FMOD_STUDIO_STOP_IMMEDIATE;
			checkErrors(aShot.instance->stop(m));
			checkErrors(aShot.instance->release());
			attachedOneShots.remove(i);
			i--;
			continue;
		}
		FMOD_STUDIO_PLAYBACK_STATE s;
		checkErrors(aShot.instance->getPlaybackState(&s));
		if (s == FMOD_STUDIO_PLAYBACK_STOPPED) {
			checkErrors(aShot.instance->release());
			attachedOneShots.remove(i);
			i--;
			continue;
		}
		updateInstance3DAttributes(aShot.instance, aShot.gameObj);
	}

	// update listener position
	setListenerAttributes();

	// if events are subscribed to callbacks, update them
	runCallbacks();

	// finally, dispatch an update call to FMOD
	checkErrors(system->update());
}

void Fmod::updateInstance3DAttributes(FMOD::Studio::EventInstance *instance, Object *o) {
	// try to set 3D attributes
	if (instance && !isNull(o)) {
		CanvasItem *ci = Object::cast_to<CanvasItem>(o);
		if (ci != nullptr) {
			Transform2D t2d = ci->get_transform();
			Vector2 posVector = t2d.get_origin() / distanceScale;
			// in 2D the distance is measured in pixels and position translates to screen coords
			// so we don't have to utilise FMOD's Y axis
			Vector3 pos(posVector.x, 0.0f, posVector.y),
					up(0, 1, 0), forward(0, 0, 1), vel(0, 0, 0);
			FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
			checkErrors(instance->set3DAttributes(&attr));
		} else {
			// needs testing
			Spatial *s = Object::cast_to<Spatial>(o);
			Transform t = s->get_transform();
			Vector3 pos = t.get_origin() / distanceScale;
			Vector3 up = t.get_basis().elements[1];
			Vector3 forward = t.get_basis().elements[2];
			Vector3 vel(0, 0, 0);
			FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
			checkErrors(instance->set3DAttributes(&attr));
		}
	}
}

void Fmod::shutdown() {
	checkErrors(system->unloadAll());
	checkErrors(system->release());
}

void Fmod::setListenerAttributes() {
	if (isNull(listener)) {
		if (nullListenerWarning) {
			print_error("FMOD Sound System: Listener not set!");
			nullListenerWarning = false;
		}
		return;
	}
	CanvasItem *ci = Object::cast_to<CanvasItem>(listener);
	if (ci != nullptr) {
		Transform2D t2d = ci->get_transform();
		Vector2 posVector = t2d.get_origin() / distanceScale;
		// in 2D the distance is measured in pixels and position translates to screen coords
		// so we don't have to utilise FMOD's Y axis
		Vector3 pos(posVector.x, 0.0f, posVector.y),
				up(0, 1, 0), forward(0, 0, 1), vel(0, 0, 0); // TODO: add doppler
		FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
		checkErrors(system->setListenerAttributes(0, &attr));

	} else {
		// needs testing
		Spatial *s = Object::cast_to<Spatial>(listener);
		Transform t = s->get_transform();
		Vector3 pos = t.get_origin() / distanceScale;
		Vector3 up = t.get_basis().elements[1];
		Vector3 forward = t.get_basis().elements[2];
		Vector3 vel(0, 0, 0);
		FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
		checkErrors(system->setListenerAttributes(0, &attr));
	}
}

void Fmod::addListener(Object *gameObj) {
	listener = gameObj;
}

void Fmod::setSoftwareFormat(int sampleRate, int speakerMode, int numRawSpeakers) {
	auto m = static_cast<FMOD_SPEAKERMODE>(speakerMode);
	checkErrors(coreSystem->setSoftwareFormat(sampleRate, m, numRawSpeakers));
}

void Fmod::setGlobalParameter(const String &parameterName, float value) {
	checkErrors(system->setParameterByName(parameterName.ascii().get_data(), value));
}

float Fmod::getGlobalParameter(const String &parameterName) {
	float value = 0.f;
	checkErrors(system->getParameterByName(parameterName.ascii().get_data(), &value));
	return value;
}

Array Fmod::getAvailableDrivers() {
	Array driverList;
	int numDrivers = 0;

	checkErrors(coreSystem->getNumDrivers(&numDrivers));

	for (int i = 0; i < numDrivers; i++) {
		char name[256];
		int sampleRate;
		FMOD_SPEAKERMODE speakerMode;
		int speakerModeChannels;
		checkErrors(coreSystem->getDriverInfo(i, name, 256, nullptr, &sampleRate, &speakerMode, &speakerModeChannels));
		String nameStr(name);

		Dictionary driverInfo;
		driverInfo["id"] = i;
		driverInfo["name"] = nameStr;
		driverInfo["sample_rate"] = sampleRate;
		driverInfo["speaker_mode"] = (int)speakerMode;
		driverInfo["number_of_channels"] = speakerModeChannels;
		driverList.push_back(driverInfo);
	}

	return driverList;
}

int Fmod::getDriver() {
	int driverId = 0;
	checkErrors(coreSystem->getDriver(&driverId));
	return driverId;
}

void Fmod::setDriver(int id) {
	checkErrors(coreSystem->setDriver(id));
}

Dictionary Fmod::getPerformanceData() {
	Dictionary performanceData;

	// get the CPU usage
	FMOD_STUDIO_CPU_USAGE cpuUsage;
	checkErrors(system->getCPUUsage(&cpuUsage));
	Dictionary cpuPerfData;
	cpuPerfData["dsp"] = cpuUsage.dspusage;
	cpuPerfData["geometry"] = cpuUsage.geometryusage;
	cpuPerfData["stream"] = cpuUsage.streamusage;
	cpuPerfData["studio"] = cpuUsage.studiousage;
	cpuPerfData["update"] = cpuUsage.updateusage;
	performanceData["CPU"] = cpuPerfData;

	// get the memory usage
	int currentAlloc = 0;
	int maxAlloc = 0;
	checkErrors(FMOD::Memory_GetStats(&currentAlloc, &maxAlloc));
	Dictionary memPerfData;
	memPerfData["currently_allocated"] = currentAlloc;
	memPerfData["max_allocated"] = maxAlloc;
	performanceData["memory"] = memPerfData;

	// get the file usage
	int64_t sampleBytesRead = 0;
	int64_t streamBytesRead = 0;
	int64_t otherBytesRead = 0;
	checkErrors(coreSystem->getFileUsage(&sampleBytesRead, &streamBytesRead, &otherBytesRead));
	Dictionary filePerfData;
	filePerfData["sample_bytes_read"] = sampleBytesRead;
	filePerfData["stream_bytes_read"] = streamBytesRead;
	filePerfData["other_bytes_read"] = otherBytesRead;
	performanceData["file"] = filePerfData;

	return performanceData;
}

void Fmod::waitForAllLoads() {
	checkErrors(system->flushSampleLoading());
}

String Fmod::loadbank(const String &pathToBank, int flags) {
	if (banks.has(pathToBank)) return pathToBank; // bank is already loaded
	FMOD::Studio::Bank *bank = nullptr;
	checkErrors(system->loadBankFile(pathToBank.ascii().get_data(), flags, &bank));
	if (bank) {
		banks.insert(pathToBank, bank);
		return pathToBank;
	}
	return pathToBank;
}

void Fmod::unloadBank(const String &pathToBank) {
	if (!banks.has(pathToBank)) return; // bank is not loaded
	auto bank = banks.find(pathToBank);
	if (bank->value()) {
		checkErrors(bank->value()->unload());
		banks.erase(pathToBank);
	}
}

int Fmod::getBankLoadingState(const String &pathToBank) {
	if (!banks.has(pathToBank)) return -1; // bank is not loaded
	auto bank = banks.find(pathToBank);
	if (bank->value()) {
		FMOD_STUDIO_LOADING_STATE state;
		checkErrors(bank->value()->getLoadingState(&state));
		return state;
	}
	return -1;
}

int Fmod::getBankBusCount(const String &pathToBank) {
	if (banks.has(pathToBank)) {
		int count;
		auto bank = banks.find(pathToBank);
		if (bank->value()) checkErrors(bank->value()->getBusCount(&count));
		return count;
	}
	return -1;
}

int Fmod::getBankEventCount(const String &pathToBank) {
	if (banks.has(pathToBank)) {
		int count;
		auto bank = banks.find(pathToBank);
		if (bank->value()) checkErrors(bank->value()->getEventCount(&count));
		return count;
	}
	return -1;
}

int Fmod::getBankStringCount(const String &pathToBank) {
	if (banks.has(pathToBank)) {
		int count;
		auto bank = banks.find(pathToBank);
		if (bank->value()) checkErrors(bank->value()->getStringCount(&count));
		return count;
	}
	return -1;
}

int Fmod::getBankVCACount(const String &pathToBank) {
	if (banks.has(pathToBank)) {
		int count;
		auto bank = banks.find(pathToBank);
		if (bank->value()) checkErrors(bank->value()->getVCACount(&count));
		return count;
	}
	return -1;
}

uint64_t Fmod::createEventInstance(const String &eventPath) {
	if (!eventDescriptions.has(eventPath)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		checkErrors(system->getEvent(eventPath.ascii().get_data(), &desc));
		eventDescriptions.insert(eventPath, desc);
	}
	auto desc = eventDescriptions.find(eventPath);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance) {
		uint64_t instanceId = (uint64_t)instance;
		unmanagedEvents.insert(instanceId, instance);
		return instanceId;
	}
	return 0;
}

float Fmod::getEventParameter(uint64_t instanceId, const String &parameterName) {
	float p = -1;
	if (!unmanagedEvents.has(instanceId)) return p;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value())
		checkErrors(i->value()->getParameterByName(parameterName.ascii().get_data(), &p));
	return p;
}

void Fmod::setEventParameter(uint64_t instanceId, const String &parameterName, float value) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setParameterByName(parameterName.ascii().get_data(), value));
}

void Fmod::releaseEvent(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) {
		checkErrors(i->value()->release());
		unmanagedEvents.erase(instanceId);
	}
}

void Fmod::startEvent(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->start());
}

void Fmod::stopEvent(uint64_t instanceId, int stopMode) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) {
		auto m = static_cast<FMOD_STUDIO_STOP_MODE>(stopMode);
		checkErrors(i->value()->stop(m));
	}
}

void Fmod::triggerEventCue(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->triggerCue());
}

int Fmod::getEventPlaybackState(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId))
		return -1;
	else {
		auto i = unmanagedEvents.find(instanceId);
		if (i->value()) {
			FMOD_STUDIO_PLAYBACK_STATE s;
			checkErrors(i->value()->getPlaybackState(&s));
			return s;
		}
		return -1;
	}
}

bool Fmod::getEventPaused(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return false;
	auto i = unmanagedEvents.find(instanceId);
	bool paused = false;
	if (i->value()) checkErrors(i->value()->getPaused(&paused));
	return paused;
}

void Fmod::setEventPaused(uint64_t instanceId, bool paused) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setPaused(paused));
}

float Fmod::getEventPitch(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return 0.0f;
	auto i = unmanagedEvents.find(instanceId);
	float pitch = 0.0f;
	if (i->value()) checkErrors(i->value()->getPitch(&pitch));
	return pitch;
}

void Fmod::setEventPitch(uint64_t instanceId, float pitch) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setPitch(pitch));
}

float Fmod::getEventVolume(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return 0.0f;
	auto i = unmanagedEvents.find(instanceId);
	float volume = 0.0f;
	if (i->value()) checkErrors(i->value()->getVolume(&volume));
	return volume;
}

void Fmod::setEventVolume(uint64_t instanceId, float volume) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setVolume(volume));
}

int Fmod::getEventTimelinePosition(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return 0;
	auto i = unmanagedEvents.find(instanceId);
	int tp = 0;
	if (i->value()) checkErrors(i->value()->getTimelinePosition(&tp));
	return tp;
}

void Fmod::setEventTimelinePosition(uint64_t instanceId, int position) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setTimelinePosition(position));
}

float Fmod::getEventReverbLevel(uint64_t instanceId, int index) {
	if (!unmanagedEvents.has(instanceId)) return 0.0f;
	auto i = unmanagedEvents.find(instanceId);
	float rvl = 0.0f;
	if (i->value()) checkErrors(i->value()->getReverbLevel(index, &rvl));
	return rvl;
}

void Fmod::setEventReverbLevel(uint64_t instanceId, int index, float level) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) checkErrors(i->value()->setReverbLevel(index, level));
}

bool Fmod::isEventVirtual(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return false;
	auto i = unmanagedEvents.find(instanceId);
	bool v = false;
	if (i->value()) checkErrors(i->value()->isVirtual(&v));
	return v;
}

bool Fmod::getBusMute(const String &busPath) {
	loadBus(busPath);
	if (!buses.has(busPath)) return false;
	bool mute = false;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->getMute(&mute));
	return mute;
}

bool Fmod::getBusPaused(const String &busPath) {
	loadBus(busPath);
	if (!buses.has(busPath)) return false;
	bool paused = false;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->getPaused(&paused));
	return paused;
}

float Fmod::getBusVolume(const String &busPath) {
	loadBus(busPath);
	if (!buses.has(busPath)) return 0.0f;
	float volume = 0.0f;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->getVolume(&volume));
	return volume;
}

void Fmod::setBusMute(const String &busPath, bool mute) {
	loadBus(busPath);
	if (!buses.has(busPath)) return;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->setMute(mute));
}

void Fmod::setBusPaused(const String &busPath, bool paused) {
	loadBus(busPath);
	if (!buses.has(busPath)) return;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->setPaused(paused));
}

void Fmod::setBusVolume(const String &busPath, float volume) {
	loadBus(busPath);
	if (!buses.has(busPath)) return;
	auto bus = buses.find(busPath);
	checkErrors(bus->value()->setVolume(volume));
}

void Fmod::stopAllBusEvents(const String &busPath, int stopMode) {
	loadBus(busPath);
	if (!buses.has(busPath)) return;
	auto bus = buses.find(busPath);
	auto m = static_cast<FMOD_STUDIO_STOP_MODE>(stopMode);
	checkErrors(bus->value()->stopAllEvents(m));
}

int Fmod::checkErrors(FMOD_RESULT result) {
	if (result != FMOD_OK) {
		print_error(FMOD_ErrorString(result));
		return 0;
	}
	return 1;
}

bool Fmod::isNull(Object *o) {
	CanvasItem *ci = Object::cast_to<CanvasItem>(o);
	Spatial *s = Object::cast_to<Spatial>(o);
	if (ci == nullptr && s == nullptr)
		// an object cannot be 2D and 3D at the same time
		// which means if the first cast returned null then the second cast also returned null
		return true;
	return false; // all g.
}

void Fmod::loadBus(const String &busPath) {
	if (!buses.has(busPath)) {
		FMOD::Studio::Bus *b = nullptr;
		checkErrors(system->getBus(busPath.ascii().get_data(), &b));
		if (b) buses.insert(busPath, b);
	}
}

void Fmod::loadVCA(const String &VCAPath) {
	if (!VCAs.has(VCAPath)) {
		FMOD::Studio::VCA *vca = nullptr;
		checkErrors(system->getVCA(VCAPath.ascii().get_data(), &vca));
		if (vca) VCAs.insert(VCAPath, vca);
	}
}

FMOD_VECTOR Fmod::toFmodVector(Vector3 vec) {
	FMOD_VECTOR fv;
	fv.x = vec.x;
	fv.y = vec.y;
	fv.z = vec.z;
	return fv;
}

FMOD_3D_ATTRIBUTES Fmod::get3DAttributes(FMOD_VECTOR pos, FMOD_VECTOR up, FMOD_VECTOR forward, FMOD_VECTOR vel) {
	FMOD_3D_ATTRIBUTES f3d;
	f3d.forward = forward;
	f3d.position = pos;
	f3d.up = up;
	f3d.velocity = vel;
	return f3d;
}

void Fmod::playOneShot(const String &eventName, Object *gameObj) {
	if (!eventDescriptions.has(eventName)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		checkErrors(system->getEvent(eventName.ascii().get_data(), &desc));
		eventDescriptions.insert(eventName, desc);
	}
	auto desc = eventDescriptions.find(eventName);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance) {
		// set 3D attributes once
		if (!isNull(gameObj)) {
			updateInstance3DAttributes(instance, gameObj);
		}
		checkErrors(instance->start());
		oneShotInstances.push_back(instance);
	}
}

void Fmod::playOneShotWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters) {
	if (!eventDescriptions.has(eventName)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		checkErrors(system->getEvent(eventName.ascii().get_data(), &desc));
		eventDescriptions.insert(eventName, desc);
	}
	auto desc = eventDescriptions.find(eventName);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance) {
		// set 3D attributes once
		if (!isNull(gameObj)) {
			updateInstance3DAttributes(instance, gameObj);
		}
		// set the initial parameter values
		auto keys = parameters.keys();
		for (int i = 0; i < keys.size(); i++) {
			String k = keys[i];
			float v = parameters[keys[i]];
			checkErrors(instance->setParameterByName(k.ascii().get_data(), v));
		}
		checkErrors(instance->start());
		oneShotInstances.push_back(instance);
	}
}

void Fmod::playOneShotAttached(const String &eventName, Object *gameObj) {
	if (!eventDescriptions.has(eventName)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		checkErrors(system->getEvent(eventName.ascii().get_data(), &desc));
		eventDescriptions.insert(eventName, desc);
	}
	auto desc = eventDescriptions.find(eventName);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance && !isNull(gameObj)) {
		AttachedOneShot aShot = { instance, gameObj };
		attachedOneShots.push_back(aShot);
		checkErrors(instance->start());
	}
}

void Fmod::playOneShotAttachedWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters) {
	if (!eventDescriptions.has(eventName)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		checkErrors(system->getEvent(eventName.ascii().get_data(), &desc));
		eventDescriptions.insert(eventName, desc);
	}
	auto desc = eventDescriptions.find(eventName);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance && !isNull(gameObj)) {
		AttachedOneShot aShot = { instance, gameObj };
		attachedOneShots.push_back(aShot);
		// set the initial parameter values
		auto keys = parameters.keys();
		for (int i = 0; i < keys.size(); i++) {
			String k = keys[i];
			float v = parameters[keys[i]];
			checkErrors(instance->setParameterByName(k.ascii().get_data(), v));
		}
		checkErrors(instance->start());
	}
}

void Fmod::attachInstanceToNode(uint64_t instanceId, Object *gameObj) {
	if (!unmanagedEvents.has(instanceId) || isNull(gameObj)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value()) {
		AttachedOneShot aShot = { i->value(), gameObj };
		attachedOneShots.push_back(aShot);
	}
}

void Fmod::detachInstanceFromNode(uint64_t instanceId) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto instance = unmanagedEvents.find(instanceId);
	if (instance->value()) {
		for (int i = 0; attachedOneShots.size(); i++) {
			auto attachedInstance = attachedOneShots.get(i).instance;
			if (attachedInstance == instance->value()) {
				attachedOneShots.remove(i);
				break;
			}
		}
	}
}

void Fmod::pauseAllEvents() {
	if (banks.size() > 1) {
		FMOD::Studio::Bus *masterBus = nullptr;
		if (checkErrors(system->getBus("bus:/", &masterBus))) {
			masterBus->setPaused(true);
		}
	}
}

void Fmod::unpauseAllEvents() {
	if (banks.size() > 1) {
		FMOD::Studio::Bus *masterBus = nullptr;
		if (checkErrors(system->getBus("bus:/", &masterBus))) {
			masterBus->setPaused(false);
		}
	}
}

void Fmod::muteAllEvents() {
	if (banks.size() > 1) {
		FMOD::Studio::Bus *masterBus = nullptr;
		if (checkErrors(system->getBus("bus:/", &masterBus))) {
			masterBus->setMute(true);
		}
	}
}

void Fmod::unmuteAllEvents() {
	if (banks.size() > 1) {
		FMOD::Studio::Bus *masterBus = nullptr;
		if (checkErrors(system->getBus("bus:/", &masterBus))) {
			masterBus->setMute(false);
		}
	}
}

bool Fmod::banksStillLoading() {
	for (auto e = banks.front(); e; e = e->next()) {
		auto bank = e->get();
		FMOD_STUDIO_LOADING_STATE s;
		checkErrors(bank->getLoadingState(&s));
		if (s == FMOD_STUDIO_LOADING_STATE_LOADING) {
			return true;
		}
	}
	return false;
}

float Fmod::getVCAVolume(const String &VCAPath) {
	loadVCA(VCAPath);
	if (!VCAs.has(VCAPath)) return 0.0f;
	auto vca = VCAs.find(VCAPath);
	float volume = 0.0f;
	checkErrors(vca->value()->getVolume(&volume));
	return volume;
}

void Fmod::setVCAVolume(const String &VCAPath, float volume) {
	loadVCA(VCAPath);
	if (!VCAs.has(VCAPath)) return;
	auto vca = VCAs.find(VCAPath);
	checkErrors(vca->value()->setVolume(volume));
}

void Fmod::playSound(uint64_t instanceId) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		checkErrors(c->setPaused(false));
	}
}

void Fmod::setSoundPaused(uint64_t instanceId, bool paused) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		checkErrors(c->setPaused(paused));
	}
}

void Fmod::stopSound(uint64_t instanceId) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		checkErrors(c->stop());
	}
}

bool Fmod::isSoundPlaying(uint64_t instanceId) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		bool isPlaying = false;
		checkErrors(c->isPlaying(&isPlaying));
		return isPlaying;
	}
	return false;
}

void Fmod::setSoundVolume(uint64_t instanceId, float volume) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		checkErrors(c->setVolume(volume));
	}
}

float Fmod::getSoundVolume(uint64_t instanceId) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		float volume = 0.f;
		checkErrors(c->getVolume(&volume));
		return volume;
	}
	return 0.f;
}

float Fmod::getSoundPitch(uint64_t instanceId) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		float pitch = 0.f;
		checkErrors(c->getPitch(&pitch));
		return pitch;
	}
	return 0.f;
}

void Fmod::setSoundPitch(uint64_t instanceId, float pitch) {
	if (sounds.has(instanceId)) {
		auto s = sounds.find(instanceId)->value();
		auto c = channels.find(s)->value();
		checkErrors(c->setPitch(pitch));
	}
}

uint64_t Fmod::loadSound(const String &path, int mode) {
	FMOD::Sound *sound = nullptr;
	checkErrors(coreSystem->createSound(path.ascii().get_data(), mode, nullptr, &sound));
	if (sound) {
		uint64_t instanceId = (uint64_t)sound;
		sounds.insert(instanceId, sound);
		FMOD::Channel *channel = nullptr;
		checkErrors(coreSystem->playSound(sound, nullptr, true, &channel));
		if (channel) {
			channels.insert(sound, channel);
			return instanceId;
		}
	}
	return 0;
}

void Fmod::releaseSound(uint64_t instanceId) {
	if (!sounds.has(instanceId)) return; // sound is not loaded
	auto sound = sounds.find(instanceId);
	if (sound->value()) {
		checkErrors(sound->value()->release());
		sounds.erase(instanceId);
	}
}

void Fmod::setSound3DSettings(float dopplerScale, float distanceFactor, float rollOffScale) {
	if (distanceFactor > 0 && checkErrors(coreSystem->set3DSettings(dopplerScale, distanceFactor, rollOffScale))) {
		distanceScale = distanceFactor;
		print_line("FMOD Sound System: Successfully set global 3D settings");
	} else {
		print_error("FMOD Sound System: Failed to set 3D settings :|");
	}
}

void Fmod::setCallback(uint64_t instanceId, int callbackMask) {
	if (!unmanagedEvents.has(instanceId)) return;
	auto i = unmanagedEvents.find(instanceId);
	if (i->value() && checkErrors(i->value()->setCallback(Callbacks::eventCallback, callbackMask))) {
		Callbacks::eventCallbacks.insert(instanceId, Callbacks::CallbackInfo());
	}
}

// runs on the Studio update thread, not the game thread
FMOD_RESULT F_CALLBACK Callbacks::eventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE *event, void *parameters) {

	FMOD::Studio::EventInstance *instance = (FMOD::Studio::EventInstance *)event;
	auto instanceId = (uint64_t)instance;
	auto callbackInfo = eventCallbacks.find(instanceId)->get();

	if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER) {
		FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES *props = (FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES *)parameters;
		mut->lock();
		callbackInfo.markerCallbackInfo["event_id"] = instanceId;
		callbackInfo.markerCallbackInfo["name"] = props->name;
		callbackInfo.markerCallbackInfo["position"] = props->position;
		callbackInfo.markerCallbackInfo["emitted"] = true;
		mut->unlock();

	} else if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT) {
		FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES *props = (FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES *)parameters;
		mut->lock();
		callbackInfo.beatCallbackInfo["event_id"] = instanceId;
		callbackInfo.beatCallbackInfo["beat"] = props->beat;
		callbackInfo.beatCallbackInfo["bar"] = props->bar;
		callbackInfo.beatCallbackInfo["tempo"] = props->tempo;
		callbackInfo.beatCallbackInfo["time_signature_upper"] = props->timesignatureupper;
		callbackInfo.beatCallbackInfo["time_signature_lower"] = props->timesignaturelower;
		callbackInfo.beatCallbackInfo["position"] = props->position;
		callbackInfo.beatCallbackInfo["emitted"] = true;
		mut->unlock();
	}

	if (type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_PLAYED || type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_STOPPED) {
		FMOD::Sound *sound = (FMOD::Sound *)parameters;
		char n[256];
		sound->getName(n, 256);
		String name(n);
		String mType = type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_PLAYED ? "played" : "stopped";
		mut->lock();
		callbackInfo.soundCallbackInfo["name"] = name;
		callbackInfo.soundCallbackInfo["type"] = mType;
		callbackInfo.soundCallbackInfo["emitted"] = true;
		mut->unlock();
	}

	return FMOD_OK;
}

// runs on the game thread
void Fmod::runCallbacks() {
	Callbacks::mut->lock();
	for (auto e = Callbacks::eventCallbacks.front(); e; e = e->next()) {
		auto cbInfo = e->get();

		// check for Marker callbacks
		if (cbInfo.markerCallbackInfo["emitted"]) {
			cbInfo.markerCallbackInfo.erase("emitted");
			emit_signal("timeline_marker", cbInfo.markerCallbackInfo);
			cbInfo.markerCallbackInfo["emitted"] = false;
		}

		// check for Beat callbacks
		if (cbInfo.beatCallbackInfo["emitted"]) {
			cbInfo.beatCallbackInfo.erase("emitted");
			emit_signal("timeline_beat", cbInfo.beatCallbackInfo);
			cbInfo.beatCallbackInfo["emitted"] = false;
		}

		// check for Sound callbacks
		if (cbInfo.soundCallbackInfo["emitted"]) {
			cbInfo.soundCallbackInfo.erase("emitted");
			if (cbInfo.soundCallbackInfo["type"] == "played")
				emit_signal("sound_played", cbInfo.soundCallbackInfo);
			else
				emit_signal("sound_stopped", cbInfo.soundCallbackInfo);
			cbInfo.soundCallbackInfo["emitted"] = false;
		}
	}
	Callbacks::mut->unlock();
}

void Fmod::_bind_methods() {
	/* system functions */
	ClassDB::bind_method(D_METHOD("system_init", "num_of_channels", "studio_flags", "flags"), &Fmod::init);
	ClassDB::bind_method(D_METHOD("system_update"), &Fmod::update);
	ClassDB::bind_method(D_METHOD("system_shutdown"), &Fmod::shutdown);
	ClassDB::bind_method(D_METHOD("system_add_listener", "node"), &Fmod::addListener);
	ClassDB::bind_method(D_METHOD("system_set_software_format", "sample_rate", "speaker_mode", "num_raw_speakers"), &Fmod::setSoftwareFormat);
	ClassDB::bind_method(D_METHOD("system_set_parameter", "name", "value"), &Fmod::setGlobalParameter);
	ClassDB::bind_method(D_METHOD("system_get_parameter", "name"), &Fmod::getGlobalParameter);
	ClassDB::bind_method(D_METHOD("system_set_sound_3d_settings", "dopplerScale", "distanceFactor", "rollOffScale"), &Fmod::setSound3DSettings);
	ClassDB::bind_method(D_METHOD("system_get_available_drivers"), &Fmod::getAvailableDrivers);
	ClassDB::bind_method(D_METHOD("system_get_driver"), &Fmod::getDriver);
	ClassDB::bind_method(D_METHOD("system_set_driver", "id"), &Fmod::setDriver);
	ClassDB::bind_method(D_METHOD("system_get_performance_data"), &Fmod::getPerformanceData);

	/* integration helper functions */
	ClassDB::bind_method(D_METHOD("play_one_shot", "event_name", "node"), &Fmod::playOneShot);
	ClassDB::bind_method(D_METHOD("play_one_shot_with_params", "event_name", "node", "initial_parameters"), &Fmod::playOneShotWithParams);
	ClassDB::bind_method(D_METHOD("play_one_shot_attached", "event_name", "node"), &Fmod::playOneShotAttached);
	ClassDB::bind_method(D_METHOD("play_one_shot_attached_with_params", "event_name", "node", "initial_parameters"), &Fmod::playOneShotAttachedWithParams);
	ClassDB::bind_method(D_METHOD("attach_instance_to_node", "id", "node"), &Fmod::attachInstanceToNode);
	ClassDB::bind_method(D_METHOD("detach_instance_from_node", "id"), &Fmod::detachInstanceFromNode);
	ClassDB::bind_method(D_METHOD("pause_all_events"), &Fmod::pauseAllEvents);
	ClassDB::bind_method(D_METHOD("unpause_all_events"), &Fmod::unpauseAllEvents);
	ClassDB::bind_method(D_METHOD("mute_all_events"), &Fmod::muteAllEvents);
	ClassDB::bind_method(D_METHOD("unmute_all_events"), &Fmod::unmuteAllEvents);
	ClassDB::bind_method(D_METHOD("banks_still_loading"), &Fmod::banksStillLoading);
	ClassDB::bind_method(D_METHOD("wait_for_all_loads"), &Fmod::waitForAllLoads);

	/* bank functions */
	ClassDB::bind_method(D_METHOD("bank_load", "path_to_bank", "flags"), &Fmod::loadbank);
	ClassDB::bind_method(D_METHOD("bank_unload", "path_to_bank"), &Fmod::unloadBank);
	ClassDB::bind_method(D_METHOD("bank_get_loading_state", "path_to_bank"), &Fmod::getBankLoadingState);
	ClassDB::bind_method(D_METHOD("bank_get_bus_count", "path_to_bank"), &Fmod::getBankBusCount);
	ClassDB::bind_method(D_METHOD("bank_get_event_count", "path_to_bank"), &Fmod::getBankEventCount);
	ClassDB::bind_method(D_METHOD("bank_get_string_count", "path_to_bank"), &Fmod::getBankStringCount);
	ClassDB::bind_method(D_METHOD("bank_get_vca_count", "path_to_bank"), &Fmod::getBankVCACount);

	/* event functions */
	ClassDB::bind_method(D_METHOD("event_create_instance", "event_path"), &Fmod::createEventInstance);
	ClassDB::bind_method(D_METHOD("event_get_parameter", "id", "parameter_name"), &Fmod::getEventParameter);
	ClassDB::bind_method(D_METHOD("event_set_parameter", "id", "parameter_name", "value"), &Fmod::setEventParameter);
	ClassDB::bind_method(D_METHOD("event_release", "id"), &Fmod::releaseEvent);
	ClassDB::bind_method(D_METHOD("event_start", "id"), &Fmod::startEvent);
	ClassDB::bind_method(D_METHOD("event_stop", "id", "stop_mode"), &Fmod::stopEvent);
	ClassDB::bind_method(D_METHOD("event_trigger_cue", "id"), &Fmod::triggerEventCue);
	ClassDB::bind_method(D_METHOD("event_get_playback_state", "id"), &Fmod::getEventPlaybackState);
	ClassDB::bind_method(D_METHOD("event_get_paused", "id"), &Fmod::getEventPaused);
	ClassDB::bind_method(D_METHOD("event_set_paused", "id", "paused"), &Fmod::setEventPaused);
	ClassDB::bind_method(D_METHOD("event_get_pitch", "id"), &Fmod::getEventPitch);
	ClassDB::bind_method(D_METHOD("event_set_pitch", "id", "pitch"), &Fmod::setEventPitch);
	ClassDB::bind_method(D_METHOD("event_get_volume", "id"), &Fmod::getEventVolume);
	ClassDB::bind_method(D_METHOD("event_set_volume", "id", "volume"), &Fmod::setEventVolume);
	ClassDB::bind_method(D_METHOD("event_get_timeline_position", "id"), &Fmod::getEventTimelinePosition);
	ClassDB::bind_method(D_METHOD("event_set_timeline_position", "id", "position"), &Fmod::setEventTimelinePosition);
	ClassDB::bind_method(D_METHOD("event_get_reverb_level", "id", "index"), &Fmod::getEventReverbLevel);
	ClassDB::bind_method(D_METHOD("event_set_reverb_level", "id", "index", "level"), &Fmod::setEventReverbLevel);
	ClassDB::bind_method(D_METHOD("event_is_virtual", "id"), &Fmod::isEventVirtual);
	ClassDB::bind_method(D_METHOD("event_set_callback", "id", "callback_mask"), &Fmod::setCallback);

	/* bus functions */
	ClassDB::bind_method(D_METHOD("bus_get_mute", "path_to_bus"), &Fmod::getBusMute);
	ClassDB::bind_method(D_METHOD("bus_get_paused", "path_to_bus"), &Fmod::getBusPaused);
	ClassDB::bind_method(D_METHOD("bus_get_volume", "path_to_bus"), &Fmod::getBusVolume);
	ClassDB::bind_method(D_METHOD("bus_set_mute", "path_to_bus", "mute"), &Fmod::setBusMute);
	ClassDB::bind_method(D_METHOD("bus_set_paused", "path_to_bus", "paused"), &Fmod::setBusPaused);
	ClassDB::bind_method(D_METHOD("bus_set_volume", "path_to_bus", "volume"), &Fmod::setBusVolume);
	ClassDB::bind_method(D_METHOD("bus_stop_all_events", "path_to_bus", "stop_mode"), &Fmod::stopAllBusEvents);

	/* VCA functions */
	ClassDB::bind_method(D_METHOD("vca_get_volume", "path_to_vca"), &Fmod::getVCAVolume);
	ClassDB::bind_method(D_METHOD("vca_set_volume", "path_to_vca", "volume"), &Fmod::setVCAVolume);

	/* Sound functions */
	ClassDB::bind_method(D_METHOD("sound_load", "path_to_sound", "mode"), &Fmod::loadSound);
	ClassDB::bind_method(D_METHOD("sound_play", "id"), &Fmod::playSound);
	ClassDB::bind_method(D_METHOD("sound_stop", "id"), &Fmod::stopSound);
	ClassDB::bind_method(D_METHOD("sound_release", "id"), &Fmod::releaseSound);
	ClassDB::bind_method(D_METHOD("sound_set_paused", "id", "paused"), &Fmod::setSoundPaused);
	ClassDB::bind_method(D_METHOD("sound_is_playing", "id"), &Fmod::isSoundPlaying);
	ClassDB::bind_method(D_METHOD("sound_set_volume", "id", "volume"), &Fmod::setSoundVolume);
	ClassDB::bind_method(D_METHOD("sound_get_volume", "id"), &Fmod::getSoundVolume);
	ClassDB::bind_method(D_METHOD("sound_set_pitch", "id", "pitch"), &Fmod::setSoundPitch);
	ClassDB::bind_method(D_METHOD("sound_get_pitch", "id"), &Fmod::getSoundPitch);

	/* Event Callback Signals */
	ADD_SIGNAL(MethodInfo("timeline_beat", PropertyInfo(Variant::DICTIONARY, "params")));
	ADD_SIGNAL(MethodInfo("timeline_marker", PropertyInfo(Variant::DICTIONARY, "params")));
	ADD_SIGNAL(MethodInfo("sound_played", PropertyInfo(Variant::DICTIONARY, "params")));
	ADD_SIGNAL(MethodInfo("sound_stopped", PropertyInfo(Variant::DICTIONARY, "params")));

	/* FMOD_INITFLAGS */
	BIND_CONSTANT(FMOD_INIT_NORMAL);
	BIND_CONSTANT(FMOD_INIT_STREAM_FROM_UPDATE);
	BIND_CONSTANT(FMOD_INIT_MIX_FROM_UPDATE);
	BIND_CONSTANT(FMOD_INIT_3D_RIGHTHANDED);
	BIND_CONSTANT(FMOD_INIT_CHANNEL_LOWPASS);
	BIND_CONSTANT(FMOD_INIT_CHANNEL_DISTANCEFILTER);
	BIND_CONSTANT(FMOD_INIT_PROFILE_ENABLE);
	BIND_CONSTANT(FMOD_INIT_VOL0_BECOMES_VIRTUAL);
	BIND_CONSTANT(FMOD_INIT_GEOMETRY_USECLOSEST);
	BIND_CONSTANT(FMOD_INIT_PREFER_DOLBY_DOWNMIX);
	BIND_CONSTANT(FMOD_INIT_THREAD_UNSAFE);
	BIND_CONSTANT(FMOD_INIT_PROFILE_METER_ALL);

	/* FMOD_STUDIO_INITFLAGS */
	BIND_CONSTANT(FMOD_STUDIO_INIT_NORMAL);
	BIND_CONSTANT(FMOD_STUDIO_INIT_LIVEUPDATE);
	BIND_CONSTANT(FMOD_STUDIO_INIT_ALLOW_MISSING_PLUGINS);
	BIND_CONSTANT(FMOD_STUDIO_INIT_SYNCHRONOUS_UPDATE);
	BIND_CONSTANT(FMOD_STUDIO_INIT_DEFERRED_CALLBACKS);
	BIND_CONSTANT(FMOD_STUDIO_INIT_LOAD_FROM_UPDATE);

	/* FMOD_STUDIO_LOAD_BANK_FLAGS */
	BIND_CONSTANT(FMOD_STUDIO_LOAD_BANK_NORMAL);
	BIND_CONSTANT(FMOD_STUDIO_LOAD_BANK_NONBLOCKING);
	BIND_CONSTANT(FMOD_STUDIO_LOAD_BANK_DECOMPRESS_SAMPLES);

	/* FMOD_STUDIO_LOADING_STATE */
	BIND_CONSTANT(FMOD_STUDIO_LOADING_STATE_UNLOADING);
	BIND_CONSTANT(FMOD_STUDIO_LOADING_STATE_LOADING);
	BIND_CONSTANT(FMOD_STUDIO_LOADING_STATE_LOADED);
	BIND_CONSTANT(FMOD_STUDIO_LOADING_STATE_ERROR);

	/* FMOD_STUDIO_PLAYBACK_STATE */
	BIND_CONSTANT(FMOD_STUDIO_PLAYBACK_PLAYING);
	BIND_CONSTANT(FMOD_STUDIO_PLAYBACK_SUSTAINING);
	BIND_CONSTANT(FMOD_STUDIO_PLAYBACK_STOPPED);
	BIND_CONSTANT(FMOD_STUDIO_PLAYBACK_STARTING);
	BIND_CONSTANT(FMOD_STUDIO_PLAYBACK_STOPPING);

	/* FMOD_STUDIO_STOP_MODE */
	BIND_CONSTANT(FMOD_STUDIO_STOP_ALLOWFADEOUT);
	BIND_CONSTANT(FMOD_STUDIO_STOP_IMMEDIATE);

	/* FMOD_STUDIO_EVENT_CALLBACK_TYPE */
	BIND_CONSTANT(FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER);
	BIND_CONSTANT(FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT);
	BIND_CONSTANT(FMOD_STUDIO_EVENT_CALLBACK_SOUND_PLAYED);
	BIND_CONSTANT(FMOD_STUDIO_EVENT_CALLBACK_SOUND_STOPPED);

	/* FMOD_SPEAKERMODE */
	BIND_CONSTANT(FMOD_SPEAKERMODE_DEFAULT);
	BIND_CONSTANT(FMOD_SPEAKERMODE_RAW);
	BIND_CONSTANT(FMOD_SPEAKERMODE_MONO);
	BIND_CONSTANT(FMOD_SPEAKERMODE_STEREO);
	BIND_CONSTANT(FMOD_SPEAKERMODE_QUAD);
	BIND_CONSTANT(FMOD_SPEAKERMODE_SURROUND);
	BIND_CONSTANT(FMOD_SPEAKERMODE_5POINT1);
	BIND_CONSTANT(FMOD_SPEAKERMODE_7POINT1);
	BIND_CONSTANT(FMOD_SPEAKERMODE_7POINT1POINT4);
	BIND_CONSTANT(FMOD_SPEAKERMODE_MAX);

	/* FMOD_MODE */
	BIND_CONSTANT(FMOD_DEFAULT);
	BIND_CONSTANT(FMOD_LOOP_OFF);
	BIND_CONSTANT(FMOD_LOOP_NORMAL);
	BIND_CONSTANT(FMOD_LOOP_BIDI);
	BIND_CONSTANT(FMOD_2D);
	BIND_CONSTANT(FMOD_3D);
	BIND_CONSTANT(FMOD_CREATESTREAM);
	BIND_CONSTANT(FMOD_CREATESAMPLE);
	BIND_CONSTANT(FMOD_CREATECOMPRESSEDSAMPLE);
	BIND_CONSTANT(FMOD_OPENUSER);
	BIND_CONSTANT(FMOD_OPENMEMORY);
	BIND_CONSTANT(FMOD_OPENMEMORY_POINT);
	BIND_CONSTANT(FMOD_OPENRAW);
	BIND_CONSTANT(FMOD_OPENONLY);
	BIND_CONSTANT(FMOD_ACCURATETIME);
	BIND_CONSTANT(FMOD_MPEGSEARCH);
	BIND_CONSTANT(FMOD_NONBLOCKING);
	BIND_CONSTANT(FMOD_UNIQUE);
	BIND_CONSTANT(FMOD_3D_HEADRELATIVE);
	BIND_CONSTANT(FMOD_3D_WORLDRELATIVE);
	BIND_CONSTANT(FMOD_3D_INVERSEROLLOFF);
	BIND_CONSTANT(FMOD_3D_LINEARROLLOFF);
	BIND_CONSTANT(FMOD_3D_LINEARSQUAREROLLOFF);
	BIND_CONSTANT(FMOD_3D_INVERSETAPEREDROLLOFF);
	BIND_CONSTANT(FMOD_3D_CUSTOMROLLOFF);
	BIND_CONSTANT(FMOD_3D_IGNOREGEOMETRY);
	BIND_CONSTANT(FMOD_IGNORETAGS);
	BIND_CONSTANT(FMOD_LOWMEM);
	BIND_CONSTANT(FMOD_VIRTUAL_PLAYFROMSTART);
}

Fmod::Fmod() {
	system = nullptr;
	coreSystem = nullptr;
	listener = nullptr;
	Callbacks::mut = Mutex::create();
	checkErrors(FMOD::Studio::System::create(&system));
	checkErrors(system->getCoreSystem(&coreSystem));
}

Fmod::~Fmod() {
	Fmod::shutdown();
	Callbacks::mut->~Mutex();
}

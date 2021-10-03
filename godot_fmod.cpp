/*************************************************************************/
/*  godot_fmod.cpp                                                       */
/*************************************************************************/
/*                                                                       */
/*       FMOD Studio module and bindings for the Godot game engine       */
/*                                                                       */
/*************************************************************************/
/* Copyright (c) 2020 Alex Fonseka                                       */
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
#include "core/os/mutex.h"

Mutex mutex;

Fmod *Fmod::singleton = nullptr;

void Fmod::init(int numOfChannels, int studioFlags, int flags) {
	// initialize FMOD Studio and FMOD Core System with provided flags
	if (checkErrors(system->initialize(numOfChannels, studioFlags, flags, nullptr))) {
		print_line("FMOD Sound System: Successfully initialized");
		if (studioFlags & FMOD_STUDIO_INIT_LIVEUPDATE)
			print_line("FMOD Sound System: Live update enabled!");
	} else
		print_error("FMOD Sound System: Failed to initialize :|");
}

void Fmod::update() {
	// clean up one shots
	for (auto e = events.front(); e; e = e->next()) {
		FMOD::Studio::EventInstance *eventInstance = e->get();
		EventInfo *eventInfo = getEventInfo(eventInstance);
		if (eventInfo->gameObj) {
			if (isNull(eventInfo->gameObj)) {
				FMOD_STUDIO_STOP_MODE m = FMOD_STUDIO_STOP_IMMEDIATE;
				checkErrors(eventInstance->stop(m));
				releaseOneEvent(eventInstance);
				continue;
			}
			updateInstance3DAttributes(eventInstance, eventInfo->gameObj);
		}
	}

	// clean up invalid channel references
	clearChannelRefs();

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
		if (ci != nullptr) { // GameObject is 2D
			Transform2D t2d = ci->get_transform();
			Vector2 posVector = t2d.get_origin() / distanceScale;
			// in 2D, the distance is measured in pixels
			// TODO: Revise the set3DAttributes call. In 2D, the emitters must directly face the listener.
			Vector3 pos(posVector.x, 0.0f, posVector.y),
					up(0, 1, 0), forward(0, 0, 1), vel(0, 0, 0); // TODO: add doppler
			FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
			checkErrors(instance->set3DAttributes(&attr));
		} else { // GameObject is 3D
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
	if (listeners.size() == 0) {
		if (listenerWarning) {
			print_error("FMOD Sound System: No listeners are set!");
			listenerWarning = false;
		}
		return;
	}

	clearNullListeners();

	for (int i = 0; i < listeners.size(); i++) {
		auto listener = listeners[i];

		CanvasItem *ci = Object::cast_to<CanvasItem>(listener.gameObj);
		if (ci != nullptr) { // Listener is in 2D space
			Transform2D t2d = ci->get_transform();
			Vector2 posVector = t2d.get_origin() / distanceScale;
			// in 2D, the distance is measured in pixels
			// TODO: Revise the set3DAttributes call. In 2D, the listener must be a few units away from
			// the emitters (or the screen) and must face them directly.
			Vector3 pos(posVector.x, 0.0f, posVector.y),
					up(0, 1, 0), forward(0, 0, 1), vel(0, 0, 0); // TODO: add doppler
			FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
			if (!listener.listenerLock) checkErrors(system->setListenerAttributes(i, &attr));

		} else { // Listener is in 3D space
			// needs testing
			Spatial *s = Object::cast_to<Spatial>(listener.gameObj);
			Transform t = s->get_transform();
			Vector3 pos = t.get_origin() / distanceScale;
			Vector3 up = t.get_basis().elements[1];
			Vector3 forward = t.get_basis().elements[2];
			Vector3 vel(0, 0, 0);
			FMOD_3D_ATTRIBUTES attr = get3DAttributes(toFmodVector(pos), toFmodVector(up), toFmodVector(forward), toFmodVector(vel));
			if (!listener.listenerLock) checkErrors(system->setListenerAttributes(i, &attr));
		}
	}
}

void Fmod::addListener(Object *gameObj) {
	if (listeners.size() == FMOD_MAX_LISTENERS) {
		print_error("FMOD Sound System: Could not add listener. System already at max listeners.");
		return;
	}
	Listener listener;
	listener.gameObj = gameObj;
	listeners.push_back(listener);
	checkErrors(system->setNumListeners(listeners.size()));
}

void Fmod::removeListener(uint8_t index) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return;
	}
	listeners.erase(listeners.begin() + index);
	checkErrors(system->setNumListeners(listeners.size() == 0 ? 1 : listeners.size()));
	std::string s = "FMOD Sound System: Listener at index " + std::to_string(index) + " was removed";
	print_line(s.c_str());
}

void Fmod::setSoftwareFormat(int sampleRate, int speakerMode, int numRawSpeakers) {
	auto m = static_cast<FMOD_SPEAKERMODE>(speakerMode);
	checkErrors(coreSystem->setSoftwareFormat(sampleRate, m, numRawSpeakers));
}

void Fmod::setGlobalParameterByName(const String &parameterName, float value) {
	checkErrors(system->setParameterByName(parameterName.ascii().get_data(), value));
}

float Fmod::getGlobalParameterByName(const String &parameterName) {
	float value = 0.f;
	checkErrors(system->getParameterByName(parameterName.ascii().get_data(), &value));
	return value;
}

void Fmod::setGlobalParameterByID(const Array &idPair, float value) {
	if (idPair.size() != 2) {
		print_error("FMOD Sound System: Invalid parameter ID");
		return;
	}
	FMOD_STUDIO_PARAMETER_ID id;
	id.data1 = idPair[0];
	id.data2 = idPair[1];
	checkErrors(system->setParameterByID(id, value));
}

float Fmod::getGlobalParameterByID(const Array &idPair) {
	if (idPair.size() != 2) {
		print_error("FMOD Sound System: Invalid parameter ID");
		return -1.f;
	}
	FMOD_STUDIO_PARAMETER_ID id;
	id.data1 = idPair[0];
	id.data2 = idPair[1];
	float value = -1.f;
	checkErrors(system->getParameterByID(id, &value));
	return value;
}

Dictionary Fmod::getGlobalParameterDescByName(const String &parameterName) {
	Dictionary paramDesc;
	FMOD_STUDIO_PARAMETER_DESCRIPTION pDesc;
	if (checkErrors(system->getParameterDescriptionByName(parameterName.ascii().get_data(), &pDesc))) {
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
	}

	return paramDesc;
}

Dictionary Fmod::getGlobalParameterDescByID(const Array &idPair) {
	if (idPair.size() != 2) {
		print_error("FMOD Sound System: Invalid parameter ID");
		return Dictionary();
	}
	Dictionary paramDesc;
	FMOD_STUDIO_PARAMETER_ID id;
	id.data1 = idPair[0];
	id.data2 = idPair[1];
	FMOD_STUDIO_PARAMETER_DESCRIPTION pDesc;
	if (checkErrors(system->getParameterDescriptionByID(id, &pDesc))) {
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
	}

	return paramDesc;
}

uint32_t Fmod::getGlobalParameterDescCount() {
	int count = 0;
	checkErrors(system->getParameterDescriptionCount(&count));
	return count;
}

Array Fmod::getGlobalParameterDescList() {
	Array a;
	FMOD_STUDIO_PARAMETER_DESCRIPTION descList[256];
	int count = 0;
	checkErrors(system->getParameterDescriptionList(descList, 256, &count));
	for (int i = 0; i < count; i++) {
		auto pDesc = descList[i];
		Dictionary paramDesc;
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
		a.append(paramDesc);
	}
	return a;
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

void Fmod::setDriver(uint8_t id) {
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
	long long sampleBytesRead = 0;
	long long streamBytesRead = 0;
	long long otherBytesRead = 0;
	checkErrors(coreSystem->getFileUsage(&sampleBytesRead, &streamBytesRead, &otherBytesRead));
	Dictionary filePerfData;
	filePerfData["sample_bytes_read"] = (uint64_t)sampleBytesRead;
	filePerfData["stream_bytes_read"] = (uint64_t)streamBytesRead;
	filePerfData["other_bytes_read"] = (uint64_t)otherBytesRead;
	performanceData["file"] = filePerfData;

	return performanceData;
}

void Fmod::setListenerLock(uint8_t index, bool isLocked) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return;
	}
	Listener *listener = &listeners[index];
	listener->listenerLock = isLocked;
}

bool Fmod::getListenerLock(uint8_t index) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return false;
	}
	return listeners[index].listenerLock;
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

uint64_t Fmod::descCreateInstance(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return 0;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	auto instance = createInstance(desc, false, nullptr);
	if (instance)
		return (uint64_t)instance;
	return 0;
}

int Fmod::descGetLength(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return -1;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	int length = 0;
	checkErrors(desc->getLength(&length));
	return length;
}

String Fmod::descGetPath(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return String("Invalid handle!");
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	char path[256];
	int retrived = 0;
	checkErrors(desc->getPath(path, 256, &retrived));
	return String(path);
}

Array Fmod::descGetInstanceList(uint64_t descHandle) {
	Array array;
	if (!ptrToEventDescMap.has(descHandle)) return array;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD::Studio::EventInstance *arr[128];
	int count = 0;
	checkErrors(desc->getInstanceList(arr, 128, &count));
	for (int i = 0; i < count; i++) {
		array.append((uint64_t)arr[i]);
	}
	return array;
}

int Fmod::descGetInstanceCount(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return -1;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	int count = 0;
	checkErrors(desc->getInstanceCount(&count));
	return count;
}

void Fmod::descReleaseAllInstances(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return;
	auto desc = ptrToEventDescMap.find(descHandle)->value();

	checkErrors(desc->releaseAllInstances());
}

void Fmod::descLoadSampleData(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	checkErrors(desc->loadSampleData());
}

void Fmod::descUnloadSampleData(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	checkErrors(desc->unloadSampleData());
}

int Fmod::descGetSampleLoadingState(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return -1;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD_STUDIO_LOADING_STATE s;
	checkErrors(desc->getSampleLoadingState(&s));
	return s;
}

bool Fmod::descIs3D(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return false;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	bool is3D = false;
	checkErrors(desc->is3D(&is3D));
	return is3D;
}

bool Fmod::descIsOneShot(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return false;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	bool isOneShot = false;
	checkErrors(desc->isOneshot(&isOneShot));
	return isOneShot;
}

bool Fmod::descIsSnapshot(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return false;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	bool isSnapshot = false;
	checkErrors(desc->isSnapshot(&isSnapshot));
	return isSnapshot;
}

bool Fmod::descIsStream(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return false;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	bool isStream = false;
	checkErrors(desc->isStream(&isStream));
	return isStream;
}

bool Fmod::descHasCue(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return false;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	bool hasCue = false;
	checkErrors(desc->hasCue(&hasCue));
	return hasCue;
}

float Fmod::descGetMaximumDistance(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return 0.f;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	float maxDist = 0.f;
	checkErrors(desc->getMaximumDistance(&maxDist));
	return maxDist;
}

float Fmod::descGetMinimumDistance(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return 0.f;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	float minDist = 0.f;
	checkErrors(desc->getMinimumDistance(&minDist));
	return minDist;
}

float Fmod::descGetSoundSize(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return 0.f;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	float soundSize = 0.f;
	checkErrors(desc->getSoundSize(&soundSize));
	return soundSize;
}

Dictionary Fmod::descGetParameterDescriptionByName(uint64_t descHandle, const String &name) {
	Dictionary paramDesc;
	if (!ptrToEventDescMap.has(descHandle)) return paramDesc;
	auto desc = ptrToEventDescMap.find(descHandle)->value();

	FMOD_STUDIO_PARAMETER_DESCRIPTION pDesc;
	if (checkErrors(desc->getParameterDescriptionByName(name.ascii().get_data(), &pDesc))) {
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
	}

	return paramDesc;
}

Dictionary Fmod::descGetParameterDescriptionByID(uint64_t descHandle, const Array &idPair) {
	Dictionary paramDesc;
	if (!ptrToEventDescMap.has(descHandle) || idPair.size() != 2) return paramDesc;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD_STUDIO_PARAMETER_ID paramId;
	paramId.data1 = (unsigned int)idPair[0];
	paramId.data2 = (unsigned int)idPair[1];
	FMOD_STUDIO_PARAMETER_DESCRIPTION pDesc;
	if (checkErrors(desc->getParameterDescriptionByID(paramId, &pDesc))) {
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
	}
	return paramDesc;
}

int Fmod::descGetParameterDescriptionCount(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return 0;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	int count = 0;
	checkErrors(desc->getParameterDescriptionCount(&count));
	return count;
}

Dictionary Fmod::descGetParameterDescriptionByIndex(uint64_t descHandle, int index) {
	Dictionary paramDesc;
	if (!ptrToEventDescMap.has(descHandle)) return paramDesc;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD_STUDIO_PARAMETER_DESCRIPTION pDesc;
	if (checkErrors(desc->getParameterDescriptionByIndex(index, &pDesc))) {
		paramDesc["name"] = String(pDesc.name);
		paramDesc["id_first"] = pDesc.id.data1;
		paramDesc["id_second"] = pDesc.id.data2;
		paramDesc["minimum"] = pDesc.minimum;
		paramDesc["maximum"] = pDesc.maximum;
		paramDesc["default_value"] = pDesc.defaultvalue;
	}
	return paramDesc;
}

Dictionary Fmod::descGetUserProperty(uint64_t descHandle, String name) {
	Dictionary propDesc;
	if (!ptrToEventDescMap.has(descHandle)) return propDesc;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD_STUDIO_USER_PROPERTY uProp;
	if (checkErrors(desc->getUserProperty(name.ascii().get_data(), &uProp))) {
		FMOD_STUDIO_USER_PROPERTY_TYPE fType = uProp.type;
		if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_INTEGER)
			propDesc[String(uProp.name)] = uProp.intvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_BOOLEAN)
			propDesc[String(uProp.name)] = (bool)uProp.boolvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_FLOAT)
			propDesc[String(uProp.name)] = uProp.floatvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_STRING)
			propDesc[String(uProp.name)] = String(uProp.stringvalue);
	}

	return propDesc;
}

int Fmod::descGetUserPropertyCount(uint64_t descHandle) {
	if (!ptrToEventDescMap.has(descHandle)) return -1;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	int count = 0;
	checkErrors(desc->getUserPropertyCount(&count));
	return count;
}

Dictionary Fmod::descUserPropertyByIndex(uint64_t descHandle, int index) {
	Dictionary propDesc;
	if (!ptrToEventDescMap.has(descHandle)) return propDesc;
	auto desc = ptrToEventDescMap.find(descHandle)->value();
	FMOD_STUDIO_USER_PROPERTY uProp;
	if (checkErrors(desc->getUserPropertyByIndex(index, &uProp))) {
		FMOD_STUDIO_USER_PROPERTY_TYPE fType = uProp.type;
		if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_INTEGER)
			propDesc[String(uProp.name)] = uProp.intvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_BOOLEAN)
			propDesc[String(uProp.name)] = (bool)uProp.boolvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_FLOAT)
			propDesc[String(uProp.name)] = uProp.floatvalue;
		else if (fType == FMOD_STUDIO_USER_PROPERTY_TYPE_STRING)
			propDesc[String(uProp.name)] = String(uProp.stringvalue);
	}

	return propDesc;
}

uint64_t Fmod::createEventInstance(const String &eventPath) {
	FMOD::Studio::EventInstance *instance = createInstance(eventPath, false, nullptr);
	if (instance) {
		uint64_t instanceId = (uint64_t)instance;
		events.insert(instanceId, instance);
		return instanceId;
	}
	return 0;
}

FMOD::Studio::EventInstance *Fmod::createInstance(const String eventPath, const bool isOneShot, Object *gameObject) {
	if (!eventDescriptions.has(eventPath)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		auto res = checkErrors(system->getEvent(eventPath.ascii().get_data(), &desc));
		if (!res) return 0;
		eventDescriptions.insert(eventPath, desc);
	}
	auto desc = eventDescriptions.find(eventPath);
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->value()->createInstance(&instance));
	if (instance && (!isOneShot || gameObject)) {
		auto *eventInfo = new EventInfo();
		eventInfo->gameObj = gameObject;
		instance->setUserData(eventInfo);
		auto instanceId = (uint64_t)instance;
		events[instanceId] = instance;
	}
	return instance;
}

FMOD::Studio::EventInstance *Fmod::createInstance(FMOD::Studio::EventDescription *eventDesc, bool isOneShot, Object *gameObject) {
	auto desc = eventDesc;
	FMOD::Studio::EventInstance *instance;
	checkErrors(desc->createInstance(&instance));
	if (instance && (!isOneShot || gameObject)) {
		auto *eventInfo = new EventInfo();
		eventInfo->gameObj = gameObject;
		instance->setUserData(eventInfo);
		auto instanceId = (uint64_t)instance;
		events[instanceId] = instance;
	}
	return instance;
}

float Fmod::getEventParameterByName(uint64_t instanceId, const String &parameterName) {
	float p = -1;
	if (!events.has(instanceId)) return p;
	auto i = events.find(instanceId);
	if (i->value())
		checkErrors(i->value()->getParameterByName(parameterName.ascii().get_data(), &p));
	return p;
}

void Fmod::setEventParameterByName(uint64_t instanceId, const String &parameterName, float value) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->setParameterByName(parameterName.ascii().get_data(), value));
}

float Fmod::getEventParameterByID(uint64_t instanceId, const Array &idPair) {
	if (!events.has(instanceId) || idPair.size() != 2) return -1.0f;
	auto i = events.find(instanceId);
	if (i->value()) {
		FMOD_STUDIO_PARAMETER_ID id;
		id.data1 = idPair[0];
		id.data2 = idPair[1];
		float value;
		checkErrors(i->value()->getParameterByID(id, &value));
		return value;
	}
	return -1.0f;
}

void Fmod::setEventParameterByID(uint64_t instanceId, const Array &idPair, float value) {
	if (!events.has(instanceId) || idPair.size() != 2) return;
	auto i = events.find(instanceId);
	if (i->value()) {
		FMOD_STUDIO_PARAMETER_ID id;
		id.data1 = idPair[0];
		id.data2 = idPair[1];
		checkErrors(i->value()->setParameterByID(id, value));
	}
}

void Fmod::releaseEvent(uint64_t instanceId) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	FMOD::Studio::EventInstance *event = i->value();
	if (event) {
		releaseOneEvent(event);
	}
}

void Fmod::releaseOneEvent(FMOD::Studio::EventInstance *eventInstance) {
	mutex.lock();
	EventInfo *eventInfo = getEventInfo(eventInstance);
	eventInstance->setUserData(nullptr);
	events.erase((uint64_t)eventInstance);
	checkErrors(eventInstance->release());
	delete &eventInfo;
	mutex.unlock();
}

void Fmod::clearNullListeners() {
	std::vector<uint32_t> queue;
	for (int i = 0; i < listeners.size(); i++) {
		if (isNull(listeners[i].gameObj)) {
			queue.push_back(i);
			std::string s = "FMOD Sound System: Listener at index " + std::to_string(i) + " was freed.";
			print_line(s.c_str());
		}
	}
	for (int i = 0; i < queue.size(); i++) {
		int index = queue[i];
		if (i != 0) index--;
		listeners.erase(listeners.begin() + index);
	}
	checkErrors(system->setNumListeners(listeners.size() == 0 ? 1 : listeners.size()));
}

void Fmod::clearChannelRefs() {
	if (channels.size() == 0) return;

	std::vector<uint64_t> refs;
	for (auto e = channels.front(); e; e = e->next()) {
		// Check if the channel is valid by calling any of its getters
		bool isPaused = false;
		FMOD_RESULT res = e->get()->getPaused(&isPaused);
		if (res != FMOD_OK)
			refs.push_back(e->key());
	}
	for (auto ref : refs)
		channels.erase(ref);
}

void Fmod::startEvent(uint64_t instanceId) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->start());
}

void Fmod::stopEvent(uint64_t instanceId, int stopMode) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) {
		auto m = static_cast<FMOD_STUDIO_STOP_MODE>(stopMode);
		checkErrors(i->value()->stop(m));
	}
}

void Fmod::triggerEventCue(uint64_t instanceId) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->triggerCue());
}

int Fmod::getEventPlaybackState(uint64_t instanceId) {
	if (!events.has(instanceId))
		return -1;
	else {
		auto i = events.find(instanceId);
		if (i->value()) {
			FMOD_STUDIO_PLAYBACK_STATE s;
			checkErrors(i->value()->getPlaybackState(&s));
			return s;
		}
		return -1;
	}
}

bool Fmod::getEventPaused(uint64_t instanceId) {
	if (!events.has(instanceId)) return false;
	auto i = events.find(instanceId);
	bool paused = false;
	if (i->value()) checkErrors(i->value()->getPaused(&paused));
	return paused;
}

void Fmod::setEventPaused(uint64_t instanceId, bool paused) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->setPaused(paused));
}

float Fmod::getEventPitch(uint64_t instanceId) {
	if (!events.has(instanceId)) return 0.0f;
	auto i = events.find(instanceId);
	float pitch = 0.0f;
	if (i->value()) checkErrors(i->value()->getPitch(&pitch));
	return pitch;
}

void Fmod::setEventPitch(uint64_t instanceId, float pitch) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->setPitch(pitch));
}

float Fmod::getEventVolume(uint64_t instanceId) {
	if (!events.has(instanceId)) return 0.0f;
	auto i = events.find(instanceId);
	float volume = 0.0f;
	FMOD::Studio::EventInstance *event = i->value();
	checkErrors(event->getVolume(&volume));
	return volume;
}

void Fmod::setEventVolume(uint64_t instanceId, float volume) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	FMOD::Studio::EventInstance *event = i->value();
	checkErrors(event->setVolume(volume));
}

int Fmod::getEventTimelinePosition(uint64_t instanceId) {
	if (!events.has(instanceId)) return 0;
	auto i = events.find(instanceId);
	int tp = 0;
	if (i->value()) checkErrors(i->value()->getTimelinePosition(&tp));
	return tp;
}

void Fmod::setEventTimelinePosition(uint64_t instanceId, int position) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->setTimelinePosition(position));
}

float Fmod::getEventReverbLevel(uint64_t instanceId, int index) {
	if (!events.has(instanceId)) return 0.0f;
	auto i = events.find(instanceId);
	float rvl = 0.0f;
	if (i->value()) checkErrors(i->value()->getReverbLevel(index, &rvl));
	return rvl;
}

void Fmod::setEventReverbLevel(uint64_t instanceId, int index, float level) {
	if (!events.has(instanceId)) return;
	auto i = events.find(instanceId);
	if (i->value()) checkErrors(i->value()->setReverbLevel(index, level));
}

bool Fmod::isEventVirtual(uint64_t instanceId) {
	if (!events.has(instanceId)) return false;
	auto i = events.find(instanceId);
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

bool Fmod::isNull(Object *o) {
	CanvasItem *ci = Object::cast_to<CanvasItem>(o);
	Spatial *s = Object::cast_to<Spatial>(o);
	if (ci == nullptr && s == nullptr)
		// an object cannot be 2D and 3D at the same time
		// which means if the first cast returned null then the second cast also returned null
		return true;
	return false; // all g.
}

Fmod::EventInfo *Fmod::getEventInfo(FMOD::Studio::EventInstance *eventInstance) {
	EventInfo *eventInfo;
	eventInstance->getUserData((void **)&eventInfo);
	return eventInfo;
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
	FMOD::Studio::EventInstance *instance = createInstance(eventName, true, nullptr);
	if (instance) {
		// set 3D attributes once
		if (!isNull(gameObj)) {
			updateInstance3DAttributes(instance, gameObj);
		}
		checkErrors(instance->start());
		checkErrors(instance->release());
	}
}

void Fmod::playOneShotWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters) {
	FMOD::Studio::EventInstance *instance = createInstance(eventName, true, nullptr);
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
		checkErrors(instance->release());
	}
}

void Fmod::playOneShotAttached(const String &eventName, Object *gameObj) {
	if (!isNull(gameObj)) {
		FMOD::Studio::EventInstance *instance = createInstance(eventName, true, gameObj);
		if (instance) {
			checkErrors(instance->start());
		}
	}
}

void Fmod::playOneShotAttachedWithParams(const String &eventName, Object *gameObj, const Dictionary &parameters) {
	if (!isNull(gameObj)) {
		FMOD::Studio::EventInstance *instance = createInstance(eventName, true, gameObj);
		if (instance) {
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
}

void Fmod::attachInstanceToNode(uint64_t instanceId, Object *gameObj) {
	if (!events.has(instanceId) || isNull(gameObj)) return;
	auto i = events.find(instanceId);
	FMOD::Studio::EventInstance *event = i->value();
	if (event) {
		EventInfo *eventInfo = getEventInfo(event);
		eventInfo->gameObj = gameObj;
	}
}

void Fmod::detachInstanceFromNode(uint64_t instanceId) {
	if (!events.has(instanceId)) return;
	auto instance = events.find(instanceId);
	FMOD::Studio::EventInstance *event = instance->value();
	if (event) {
		EventInfo *eventInfo = getEventInfo(event);
		eventInfo->gameObj = nullptr;
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

uint64_t Fmod::playSound(uint64_t handle) {
	if (sounds.has(handle)) {
		auto s = sounds.find(handle)->value();
		FMOD::Channel *channel = nullptr;
		checkErrors(coreSystem->playSound(s, nullptr, true, &channel));
		if (channel) {
			checkErrors(channel->setPaused(false));
			channels.insert((uint64_t)channel, channel);
			return (uint64_t)channel;
		}
	}
	return 0;
}

void Fmod::setSoundPaused(uint64_t channelHandle, bool paused) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		checkErrors(c->setPaused(paused));
	}
}

void Fmod::stopSound(uint64_t channelHandle) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		checkErrors(c->stop());
	}
}

bool Fmod::isSoundPlaying(uint64_t channelHandle) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		bool isPlaying = false;
		checkErrors(c->isPlaying(&isPlaying));
		return isPlaying;
	}
	return false;
}

void Fmod::setSoundVolume(uint64_t channelHandle, float volume) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		checkErrors(c->setVolume(volume));
	}
}

float Fmod::getSoundVolume(uint64_t channelHandle) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		float volume = 0.f;
		checkErrors(c->getVolume(&volume));
		return volume;
	}
	return 0.f;
}

float Fmod::getSoundPitch(uint64_t channelHandle) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		float pitch = 0.f;
		checkErrors(c->getPitch(&pitch));
		return pitch;
	}
	return 0.f;
}

void Fmod::setSoundPitch(uint64_t channelHandle, float pitch) {
	if (channels.has(channelHandle)) {
		auto c = channels.find(channelHandle)->value();
		checkErrors(c->setPitch(pitch));
	}
}

uint64_t Fmod::createSound(const String &path, int mode) {
	FMOD::Sound *sound = nullptr;
	checkErrors(coreSystem->createSound(path.ascii().get_data(), mode, nullptr, &sound));
	if (sound) {
		checkErrors(sound->setLoopCount(0));
		sounds.insert((uint64_t)sound, sound);
	}

	return (uint64_t)sound;
}

void Fmod::releaseSound(uint64_t handle) {
	if (!sounds.has(handle)) {
		print_error("FMOD Sound System: Invalid handle");
		return;
	}
	auto sound = sounds.find(handle);
	if (sound->value()) {
		checkErrors(sound->value()->release());
		sounds.erase(handle);
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

int Fmod::getSystemNumListeners() {
	return listeners.size();
}

float Fmod::getSystemListenerWeight(uint8_t index) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return -1;
	}
	float weight = 0;
	checkErrors(system->getListenerWeight(index, &weight));
	return weight;
}

void Fmod::setSystemListenerWeight(uint8_t index, float weight) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return;
	}
	checkErrors(system->setListenerWeight(index, weight));
}

Dictionary Fmod::getSystemListener3DAttributes(uint8_t index) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return Dictionary();
	}
	FMOD_3D_ATTRIBUTES attr;
	checkErrors(system->getListenerAttributes(index, &attr));
	Dictionary _3Dattr;
	Vector3 forward(attr.forward.x, attr.forward.y, attr.forward.z);
	Vector3 up(attr.up.x, attr.up.y, attr.up.z);
	Vector3 position(attr.position.x, attr.position.y, attr.position.z);
	Vector3 velocity(attr.velocity.x, attr.velocity.y, attr.velocity.z);
	_3Dattr["forward"] = forward;
	_3Dattr["position"] = position;
	_3Dattr["up"] = up;
	_3Dattr["velocity"] = velocity;
	return _3Dattr;
}

void Fmod::setSystemListener3DAttributes(uint8_t index, Vector3 forward, Vector3 position, Vector3 up, Vector3 velocity) {
	if (index < 0 || index + 1 > listeners.size()) {
		print_error("FMOD Sound System: Invalid listener ID");
		return;
	}
	FMOD_3D_ATTRIBUTES attr;
	attr.forward = toFmodVector(forward);
	attr.position = toFmodVector(position);
	attr.up = toFmodVector(up);
	attr.velocity = toFmodVector(velocity);
	checkErrors(system->setListenerAttributes(index, &attr));
}

uint64_t Fmod::getEvent(const String &path) {
	if (!eventDescriptions.has(path)) {
		FMOD::Studio::EventDescription *desc = nullptr;
		auto res = checkErrors(system->getEvent(path.ascii().get_data(), &desc));
		if (!res) return 0;
		eventDescriptions.insert(path, desc);
	}
	auto desc = eventDescriptions.find(path)->value();
	auto ptr = (uint64_t)desc;
	ptrToEventDescMap.insert(ptr, desc);

	return ptr;
}

void Fmod::setCallback(uint64_t instanceId, int callbackMask) {
	if (!events.has(instanceId)) return;
	FMOD::Studio::EventInstance *event = events.find(instanceId)->value();
	if (event) {
		checkErrors(event->setCallback(Callbacks::eventCallback, callbackMask));
	}
}

uint64_t Fmod::getEventDescription(uint64_t instanceId) {
	if (!events.has(instanceId)) return 0;

	auto instance = events.find(instanceId)->value();
	FMOD::Studio::EventDescription *desc = nullptr;
	checkErrors(instance->getDescription(&desc));
	auto ptr = (uint64_t)desc;
	ptrToEventDescMap.insert(ptr, desc);

	return ptr;
}

void Fmod::setEvent3DAttributes(uint64_t instanceId, Vector3 forward, Vector3 position, Vector3 up, Vector3 velocity) {
	if (!events.has(instanceId)) return;
	auto instance = events.find(instanceId)->value();
	FMOD_3D_ATTRIBUTES attr;
	attr.forward = toFmodVector(forward);
	attr.position = toFmodVector(position);
	attr.up = toFmodVector(up);
	attr.velocity = toFmodVector(velocity);
	checkErrors(instance->set3DAttributes(&attr));
}

Dictionary Fmod::getEvent3DAttributes(uint64_t instanceId) {
	if (!events.has(instanceId)) {
		print_error("Invalid event instance handle");
		return Dictionary();
	}
	auto instance = events.find(instanceId)->value();
	FMOD_3D_ATTRIBUTES attr;
	checkErrors(instance->get3DAttributes(&attr));
	Dictionary _3Dattr;
	Vector3 forward(attr.forward.x, attr.forward.y, attr.forward.z);
	Vector3 up(attr.up.x, attr.up.y, attr.up.z);
	Vector3 position(attr.position.x, attr.position.y, attr.position.z);
	Vector3 velocity(attr.velocity.x, attr.velocity.y, attr.velocity.z);
	_3Dattr["forward"] = forward;
	_3Dattr["position"] = position;
	_3Dattr["up"] = up;
	_3Dattr["velocity"] = velocity;
	return _3Dattr;
}

void Fmod::setEventListenerMask(uint64_t instanceId, int mask) {
	if (!events.has(instanceId)) {
		print_error("Invalid event instance handle");
		return;
	}
	auto instance = events.find(instanceId)->value();
	checkErrors(instance->setListenerMask(mask));
}

uint32_t Fmod::getEventListenerMask(uint64_t instanceId) {
	if (!events.has(instanceId)) {
		print_error("Invalid event instance handle");
		return 0;
	}
	auto instance = events.find(instanceId)->value();
	uint32_t mask = 0;
	checkErrors(instance->getListenerMask(&mask));
	return mask;
}

// runs on the Studio update thread, not the game thread
FMOD_RESULT F_CALLBACK Callbacks::eventCallback(FMOD_STUDIO_EVENT_CALLBACK_TYPE type, FMOD_STUDIO_EVENTINSTANCE *event, void *parameters) {

	FMOD::Studio::EventInstance *instance = (FMOD::Studio::EventInstance *)event;
	auto instanceId = (uint64_t)instance;
	Fmod::EventInfo *eventInfo;
	mutex.lock();
	// check if instance is still valid
	if (!instance) {
		mutex.unlock();
		return FMOD_OK;
	}
	instance->getUserData((void **)&eventInfo);
	if (eventInfo) {
		Callbacks::CallbackInfo callbackInfo = eventInfo->callbackInfo;

		if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_MARKER) {
			FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES *props = (FMOD_STUDIO_TIMELINE_MARKER_PROPERTIES *)parameters;
			callbackInfo.markerCallbackInfo["event_id"] = instanceId;
			callbackInfo.markerCallbackInfo["name"] = props->name;
			callbackInfo.markerCallbackInfo["position"] = props->position;
			callbackInfo.markerSignalEmitted = false;
		} else if (type == FMOD_STUDIO_EVENT_CALLBACK_TIMELINE_BEAT) {
			FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES *props = (FMOD_STUDIO_TIMELINE_BEAT_PROPERTIES *)parameters;
			callbackInfo.beatCallbackInfo["event_id"] = instanceId;
			callbackInfo.beatCallbackInfo["beat"] = props->beat;
			callbackInfo.beatCallbackInfo["bar"] = props->bar;
			callbackInfo.beatCallbackInfo["tempo"] = props->tempo;
			callbackInfo.beatCallbackInfo["time_signature_upper"] = props->timesignatureupper;
			callbackInfo.beatCallbackInfo["time_signature_lower"] = props->timesignaturelower;
			callbackInfo.beatCallbackInfo["position"] = props->position;
			callbackInfo.beatSignalEmitted = false;
		} else if (type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_PLAYED || type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_STOPPED) {
			FMOD::Sound *sound = (FMOD::Sound *)parameters;
			char n[256];
			sound->getName(n, 256);
			String name(n);
			String mType = type == FMOD_STUDIO_EVENT_CALLBACK_SOUND_PLAYED ? "played" : "stopped";
			callbackInfo.soundCallbackInfo["name"] = name;
			callbackInfo.soundCallbackInfo["type"] = mType;
			callbackInfo.soundSignalEmitted = false;
		}
	}
	mutex.unlock();
	return FMOD_OK;
}

void Fmod::runCallbacks() {
	mutex.lock();
	for (auto e = events.front(); e; e = e->next()) {
		FMOD::Studio::EventInstance *eventInstance = e->get();
		Callbacks::CallbackInfo cbInfo = getEventInfo(eventInstance)->callbackInfo;
		// check for Marker callbacks
		if (!cbInfo.markerSignalEmitted) {
			emit_signal("timeline_marker", cbInfo.markerCallbackInfo);
			cbInfo.markerSignalEmitted = true;
		}

		// check for Beat callbacks
		if (!cbInfo.beatSignalEmitted) {
			emit_signal("timeline_beat", cbInfo.beatCallbackInfo);
			cbInfo.beatSignalEmitted = true;
		}

		// check for Sound callbacks
		if (!cbInfo.soundSignalEmitted) {
			if (cbInfo.soundCallbackInfo["type"] == "played")
				emit_signal("sound_played", cbInfo.soundCallbackInfo);
			else
				emit_signal("sound_stopped", cbInfo.soundCallbackInfo);
			cbInfo.soundSignalEmitted = true;
		}
	}
	mutex.unlock();
}

void Fmod::_bind_methods() {
	/* System functions */
	ClassDB::bind_method(D_METHOD("system_init", "num_of_channels", "studio_flags", "flags"), &Fmod::init);
	ClassDB::bind_method(D_METHOD("system_update"), &Fmod::update);
	ClassDB::bind_method(D_METHOD("system_shutdown"), &Fmod::shutdown);
	ClassDB::bind_method(D_METHOD("system_add_listener", "node"), &Fmod::addListener);
	ClassDB::bind_method(D_METHOD("system_remove_listener", "index"), &Fmod::removeListener);
	ClassDB::bind_method(D_METHOD("system_set_software_format", "sample_rate", "speaker_mode", "num_raw_speakers"), &Fmod::setSoftwareFormat);
	ClassDB::bind_method(D_METHOD("system_set_parameter_by_name", "name", "value"), &Fmod::setGlobalParameterByName);
	ClassDB::bind_method(D_METHOD("system_get_parameter_by_name", "name"), &Fmod::getGlobalParameterByName);
	ClassDB::bind_method(D_METHOD("system_set_parameter_by_id", "id_pair", "value"), &Fmod::setGlobalParameterByID);
	ClassDB::bind_method(D_METHOD("system_get_parameter_by_id", "id_pair"), &Fmod::getGlobalParameterByID);
	ClassDB::bind_method(D_METHOD("system_get_parameter_desc_by_name", "name"), &Fmod::getGlobalParameterDescByName);
	ClassDB::bind_method(D_METHOD("system_get_parameter_desc_by_id", "id_pair"), &Fmod::getGlobalParameterDescByID);
	ClassDB::bind_method(D_METHOD("system_get_parameter_desc_count"), &Fmod::getGlobalParameterDescCount);
	ClassDB::bind_method(D_METHOD("system_get_parameter_desc_list"), &Fmod::getGlobalParameterDescList);
	ClassDB::bind_method(D_METHOD("system_get_num_listeners"), &Fmod::getSystemNumListeners);
	ClassDB::bind_method(D_METHOD("system_get_listener_weight", "index"), &Fmod::getSystemListenerWeight);
	ClassDB::bind_method(D_METHOD("system_set_listener_weight", "index", "weight"), &Fmod::setSystemListenerWeight);
	ClassDB::bind_method(D_METHOD("system_get_listener_attributes", "index"), &Fmod::getSystemListener3DAttributes);
	ClassDB::bind_method(D_METHOD("system_set_listener_attributes", "index", "forward", "position", "up", "velocity"), &Fmod::setSystemListener3DAttributes);
	ClassDB::bind_method(D_METHOD("system_set_sound_3d_settings", "dopplerScale", "distanceFactor", "rollOffScale"), &Fmod::setSound3DSettings);
	ClassDB::bind_method(D_METHOD("system_get_available_drivers"), &Fmod::getAvailableDrivers);
	ClassDB::bind_method(D_METHOD("system_get_driver"), &Fmod::getDriver);
	ClassDB::bind_method(D_METHOD("system_set_driver", "id"), &Fmod::setDriver);
	ClassDB::bind_method(D_METHOD("system_get_performance_data"), &Fmod::getPerformanceData);
	ClassDB::bind_method(D_METHOD("system_get_event", "path"), &Fmod::getEvent);
	ClassDB::bind_method(D_METHOD("system_set_listener_lock", "index", "is_locked"), &Fmod::setListenerLock);
	ClassDB::bind_method(D_METHOD("system_get_listener_lock", "index"), &Fmod::getListenerLock);

	/* Integration helper functions */
	ClassDB::bind_method(D_METHOD("create_event_instance", "event_path"), &Fmod::createEventInstance);
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

	/* Bank functions */
	ClassDB::bind_method(D_METHOD("bank_load", "path_to_bank", "flags"), &Fmod::loadbank);
	ClassDB::bind_method(D_METHOD("bank_unload", "path_to_bank"), &Fmod::unloadBank);
	ClassDB::bind_method(D_METHOD("bank_get_loading_state", "path_to_bank"), &Fmod::getBankLoadingState);
	ClassDB::bind_method(D_METHOD("bank_get_bus_count", "path_to_bank"), &Fmod::getBankBusCount);
	ClassDB::bind_method(D_METHOD("bank_get_event_count", "path_to_bank"), &Fmod::getBankEventCount);
	ClassDB::bind_method(D_METHOD("bank_get_string_count", "path_to_bank"), &Fmod::getBankStringCount);
	ClassDB::bind_method(D_METHOD("bank_get_vca_count", "path_to_bank"), &Fmod::getBankVCACount);

	/* EventDescription functions */
	ClassDB::bind_method(D_METHOD("event_desc_create_instance", "desc_handle"), &Fmod::descCreateInstance);
	ClassDB::bind_method(D_METHOD("event_desc_get_length", "desc_handle"), &Fmod::descGetLength);
	ClassDB::bind_method(D_METHOD("event_desc_get_path", "desc_handle"), &Fmod::descGetPath);
	ClassDB::bind_method(D_METHOD("event_desc_get_instance_list", "desc_handle"), &Fmod::descGetInstanceList);
	ClassDB::bind_method(D_METHOD("event_desc_get_instance_count", "desc_handle"), &Fmod::descGetInstanceCount);
	ClassDB::bind_method(D_METHOD("event_desc_release_all_instances", "desc_handle"), &Fmod::descReleaseAllInstances);
	ClassDB::bind_method(D_METHOD("event_desc_load_sample_data", "desc_handle"), &Fmod::descLoadSampleData);
	ClassDB::bind_method(D_METHOD("event_desc_unload_sample_data", "desc_handle"), &Fmod::descUnloadSampleData);
	ClassDB::bind_method(D_METHOD("event_desc_get_sample_loading_state", "desc_handle"), &Fmod::descGetSampleLoadingState);
	ClassDB::bind_method(D_METHOD("event_desc_is_3D", "desc_handle"), &Fmod::descIs3D);
	ClassDB::bind_method(D_METHOD("event_desc_is_oneshot", "desc_handle"), &Fmod::descIsOneShot);
	ClassDB::bind_method(D_METHOD("event_desc_is_snapshot", "desc_handle"), &Fmod::descIsSnapshot);
	ClassDB::bind_method(D_METHOD("event_desc_is_stream", "desc_handle"), &Fmod::descIsStream);
	ClassDB::bind_method(D_METHOD("event_desc_has_cue", "desc_handle"), &Fmod::descHasCue);
	ClassDB::bind_method(D_METHOD("event_desc_get_maximum_distance", "desc_handle"), &Fmod::descGetMaximumDistance);
	ClassDB::bind_method(D_METHOD("event_desc_get_minimum_distance", "desc_handle"), &Fmod::descGetMinimumDistance);
	ClassDB::bind_method(D_METHOD("event_desc_get_sound_size", "desc_handle"), &Fmod::descGetSoundSize);
	ClassDB::bind_method(D_METHOD("event_desc_get_parameter_desc_by_name", "desc_handle", "parameter_name"), &Fmod::descGetParameterDescriptionByName);
	ClassDB::bind_method(D_METHOD("event_desc_get_parameter_desc_by_id", "desc_handle", "id_pair"), &Fmod::descGetParameterDescriptionByID);
	ClassDB::bind_method(D_METHOD("event_desc_get_parameter_description_count", "desc_handle"), &Fmod::descGetParameterDescriptionCount);
	ClassDB::bind_method(D_METHOD("event_desc_get_parameter_desc_by_index", "desc_handle", "index"), &Fmod::descGetParameterDescriptionByIndex);
	ClassDB::bind_method(D_METHOD("event_desc_get_user_property", "desc_handle", "name"), &Fmod::descGetUserProperty);
	ClassDB::bind_method(D_METHOD("event_desc_get_user_property_count", "desc_handle"), &Fmod::descGetUserPropertyCount);
	ClassDB::bind_method(D_METHOD("event_desc_get_user_property_by_index", "desc_handle", "index"), &Fmod::descGetParameterDescriptionByIndex);

	/* EventInstance functions */
	ClassDB::bind_method(D_METHOD("event_get_parameter_by_name", "handle", "parameter_name"), &Fmod::getEventParameterByName);
	ClassDB::bind_method(D_METHOD("event_set_parameter_by_name", "handle", "parameter_name", "value"), &Fmod::setEventParameterByName);
	ClassDB::bind_method(D_METHOD("event_get_parameter_by_id", "handle", "parameter_id_pair"), &Fmod::getEventParameterByID);
	ClassDB::bind_method(D_METHOD("event_set_parameter_by_id", "handle", "parameter_id_pair", "value"), &Fmod::setEventParameterByID);
	ClassDB::bind_method(D_METHOD("event_release", "handle"), &Fmod::releaseEvent);
	ClassDB::bind_method(D_METHOD("event_start", "handle"), &Fmod::startEvent);
	ClassDB::bind_method(D_METHOD("event_stop", "handle", "stop_mode"), &Fmod::stopEvent);
	ClassDB::bind_method(D_METHOD("event_trigger_cue", "handle"), &Fmod::triggerEventCue);
	ClassDB::bind_method(D_METHOD("event_get_playback_state", "handle"), &Fmod::getEventPlaybackState);
	ClassDB::bind_method(D_METHOD("event_get_paused", "handle"), &Fmod::getEventPaused);
	ClassDB::bind_method(D_METHOD("event_set_paused", "handle", "paused"), &Fmod::setEventPaused);
	ClassDB::bind_method(D_METHOD("event_get_pitch", "handle"), &Fmod::getEventPitch);
	ClassDB::bind_method(D_METHOD("event_set_pitch", "handle", "pitch"), &Fmod::setEventPitch);
	ClassDB::bind_method(D_METHOD("event_get_volume", "handle"), &Fmod::getEventVolume);
	ClassDB::bind_method(D_METHOD("event_set_volume", "handle", "volume"), &Fmod::setEventVolume);
	ClassDB::bind_method(D_METHOD("event_get_timeline_position", "handle"), &Fmod::getEventTimelinePosition);
	ClassDB::bind_method(D_METHOD("event_set_timeline_position", "handle", "position"), &Fmod::setEventTimelinePosition);
	ClassDB::bind_method(D_METHOD("event_get_reverb_level", "handle", "index"), &Fmod::getEventReverbLevel);
	ClassDB::bind_method(D_METHOD("event_set_reverb_level", "handle", "index", "level"), &Fmod::setEventReverbLevel);
	ClassDB::bind_method(D_METHOD("event_is_virtual", "handle"), &Fmod::isEventVirtual);
	ClassDB::bind_method(D_METHOD("event_set_callback", "handle", "callback_mask"), &Fmod::setCallback);
	ClassDB::bind_method(D_METHOD("event_get_description", "handle"), &Fmod::getEventDescription);
	ClassDB::bind_method(D_METHOD("event_set_3D_attributes", "handle", "forward", "position", "up", "velocity"), &Fmod::setEvent3DAttributes);
	ClassDB::bind_method(D_METHOD("event_get_3D_attributes", "handle"), &Fmod::getEvent3DAttributes);
	ClassDB::bind_method(D_METHOD("event_set_listener_mask", "handle", "mask"), &Fmod::setEventListenerMask);
	ClassDB::bind_method(D_METHOD("event_get_listener_mask", "handle"), &Fmod::getEventListenerMask);

	/* Bus functions */
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

	/* Core (Low Level) Sound functions */
	ClassDB::bind_method(D_METHOD("sound_create", "path_to_sound", "mode"), &Fmod::createSound);
	ClassDB::bind_method(D_METHOD("sound_play", "handle"), &Fmod::playSound);
	ClassDB::bind_method(D_METHOD("sound_stop", "handle"), &Fmod::stopSound);
	ClassDB::bind_method(D_METHOD("sound_release", "handle"), &Fmod::releaseSound);
	ClassDB::bind_method(D_METHOD("sound_set_paused", "channel_handle", "paused"), &Fmod::setSoundPaused);
	ClassDB::bind_method(D_METHOD("sound_is_playing", "channel_handle"), &Fmod::isSoundPlaying);
	ClassDB::bind_method(D_METHOD("sound_set_volume", "channel_handle", "volume"), &Fmod::setSoundVolume);
	ClassDB::bind_method(D_METHOD("sound_get_volume", "channel_handle"), &Fmod::getSoundVolume);
	ClassDB::bind_method(D_METHOD("sound_set_pitch", "channel_handle", "pitch"), &Fmod::setSoundPitch);
	ClassDB::bind_method(D_METHOD("sound_get_pitch", "channel_handle"), &Fmod::getSoundPitch);

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
	BIND_CONSTANT(FMOD_STUDIO_LOADING_STATE_UNLOADED);

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

Fmod *Fmod::getSingleton() {
	return singleton;
}

Fmod::Fmod() {
	singleton = this;
	system = nullptr;
	coreSystem = nullptr;
	checkErrors(FMOD::Studio::System::create(&system));
	checkErrors(system->getCoreSystem(&coreSystem));
}

Fmod::~Fmod() {
	mutex.unlock();
	singleton = nullptr;
}

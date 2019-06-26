#include "register_types.h"
#include "core/class_db.h"
#include "core/engine.h"

#include "godot_fmod.h"

static Fmod *fmodPtr = nullptr;

void register_fmod_types() {
	ClassDB::register_class<Fmod>();
	fmodPtr = memnew(Fmod);
	Engine::get_singleton()->add_singleton(Engine::Singleton("Fmod", Fmod::getSingleton()));
}

void unregister_fmod_types() {
	memdelete<Fmod>(fmodPtr);
}

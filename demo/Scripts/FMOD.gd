extends Node

func _ready():
	# set up FMOD 
	Fmod.system_set_software_format(0, Fmod.FMOD_SPEAKERMODE_STEREO, 0)
	Fmod.system_init(1024, Fmod.FMOD_STUDIO_INIT_LIVEUPDATE, Fmod.FMOD_INIT_VOL0_BECOMES_VIRTUAL)
	
	# load banks
	Fmod.bank_load("./Banks/Desktop/Master.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)
	Fmod.bank_load("./Banks/Desktop/Master.strings.bank", Fmod.FMOD_STUDIO_LOAD_BANK_NORMAL)
	
	# wait for bank loads
	Fmod.wait_for_all_loads()
	
	# connect signals
	Fmod.connect("timeline_beat", self, "_on_beat")
	Fmod.connect("timeline_marker", self, "_on_marker")
	Fmod.connect("sound_played", self, "_on_sound_played")
	Fmod.connect("sound_stopped", self, "_on_sound_stopped")
	
func _on_beat(params):	
	print(params)

func _on_marker(params):
	print(params)
	
func _on_sound_played(params):
	print(params)
	
func _on_sound_stopped(params):
	print(params)
	
#warning-ignore:unused_argument
func _process(delta):
	Fmod.system_update()

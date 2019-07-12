extends Node2D

func _ready():
	# register a listener
	Fmod.system_add_listener(0, $Listener)
	
	# play some events
	# technically these are not one-shots but this is just for demo's sake
	Fmod.play_one_shot("event:/Car engine", $SoundSource1)
	Fmod.play_one_shot("event:/Waterfall", $SoundSource2)
def can_build(env, platform):
    return platform == "x11" or platform == "windows" or platform == "osx"


def configure(env):
    if env["bits"] == "32":
        env.Append(LIBPATH=["#modules/fmod/api/core/lib/x86/",
                            "#modules/fmod/api/studio/lib/x86/"])
        env.Append(LIBS=["fmod_vc", "fmodstudio_vc"])
    else:
        env.Append(LIBPATH=["#modules/fmod/api/core/lib/x64/",
                            "#modules/fmod/api/studio/lib/x64/"])
        env.Append(LIBS=["fmod64_vc", "fmodstudio64_vc"])

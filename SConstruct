#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")

# PROJECT CONFIGURATION
# -------------------------------------------------------------------------
plugin_path = "demo/addons/terrain_server/bin/"
lib_name = "libterrain_server"
source_path = "src/"
# -------------------------------------------------------------------------

# Add source folder to includes to allow #include "nodes/..."
env.Append(CPPPATH=[source_path])

# Recursive search for source files (.cpp)
sources = []
for root, dirs, files in os.walk(source_path):
    for file in files:
        if file.endswith(".cpp"):
            sources.append(os.path.join(root, file))

# Class reference XML, compiled in so the editor can show it (F1)
if env["target"] in ["editor", "template_debug"]:
    sources.append(env.GodotCPPDocData("gen/doc_data.gen.cpp", source=Glob("doc_classes/*.xml")))

# Platform specific build configuration
if env["platform"] == "macos":
    # In macOS, we build a .framework
    library = env.SharedLibrary(
        "{}{}.{}.{}.framework/{}.{}.{}".format(
            plugin_path, lib_name, env["platform"], env["target"],
            lib_name, env["platform"], env["target"]
        ),
        source=sources,
    )
elif env["platform"] == "ios":
    if env["ios_simulator"]:
        library = env.StaticLibrary(
            "{}{}.{}.{}.simulator.a".format(
                plugin_path, lib_name, env["platform"], env["target"]
            ),
            source=sources,
        )
    else:
        library = env.StaticLibrary(
            "{}{}.{}.{}.a".format(
                plugin_path, lib_name, env["platform"], env["target"]
            ),
            source=sources,
        )
else:
    # Windows and Linux
    library = env.SharedLibrary(
        "{}{}{}{}".format(
            plugin_path, lib_name, env["suffix"], env["SHLIBSUFFIX"]
        ),
        source=sources,
    )

Default(library)
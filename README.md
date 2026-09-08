# TerrainServer

![Godot Engine](https://img.shields.io/badge/Godot-4.5+-478cbf?logo=godot-engine&logoColor=white)
![C++](https://img.shields.io/badge/Language-C++17-00599C?logo=c%2B%2B&logoColor=white)
![License](https://img.shields.io/badge/License-GNU%20GPLv3-blue.svg)
[![Build](https://github.com/mdoradom/TerrainServer/actions/workflows/build.yml/badge.svg)](https://github.com/mdoradom/TerrainServer/actions/workflows/build.yml)

> [!NOTE]
> Every time you add a new class to the plugin, re-generate `compile_commands.json` by running `scons compiledb=yes` in the terminal.

## Class reference

The in-editor class reference (F1) comes from `doc_classes/*.xml`, which SCons compiles into the
plugin binary. After adding or changing anything bound in a `_bind_methods`, refresh the
skeletons:

```sh
scons platform=linux target=template_debug                       # doctool needs the built extension
godot --headless --path demo --doctool "$PWD" --gdextension-docs  # refresh doc_classes/
```

`--doctool` **merges** rather than overwrites: existing descriptions are preserved, entries for
new bindings appear empty, entries for removed bindings disappear, and escaping is normalised.
Fill in the new `<description>` blocks by hand — property ranges and defaults are read from
`_bind_methods`, so never write a range the bindings do not declare — then rebuild to compile
them in.

> [!IMPORTANT]
> Unlike `terrain.gdshader`, which loads from `res://` at runtime, the class reference is
> **compiled into the plugin binary**. Editing an XML on its own changes nothing: every doc edit
> needs a `scons` run, and then an editor restart, because extension doc data is read once at
> editor startup.
>
> Build the target the `.gdextension` actually names —
> `linux.debug.x86_64` maps to `libterrain_server.linux.template_debug.x86_64.so`, so a
> `dev_build=yes` build lands at `...template_debug.dev.x86_64.so` and is silently never loaded.
> When docs look stale, check which `bin/*.so` you just wrote before suspecting the markup.

Docs are only compiled into `template_debug` builds, which is what the editor loads. Verify with
F1 in the editor: `--headless` does not load class docs, and `--doctool` ignores doc data
registered by an extension, so neither can confirm the result.

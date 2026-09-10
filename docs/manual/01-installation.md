# Installation

TerrainServer is a GDExtension plugin, there is nothing to compile or enable once it's in your
project, unless you're building from source.

## Requirements

Godot 4.5 or newer (tested on 4.7.1). A GDExtension bound against the 4.5 API cannot load in an
older editor.

## Install from a release

1. Download the latest release archive (for example, `terrain_server-v1.2.3.zip`) from the
   [GitHub Releases page](https://github.com/mdoradom/TerrainServer/releases).
2. Extract it into your project's root directory. This creates
   `res://addons/terrain_server/`, containing the prebuilt binaries for Linux, Windows and
   macOS plus the `.gdextension` file that tells Godot how to load them.
3. Restart Godot (or open the project for the first time). The extension loads automatically,
   there is no entry under **Project > Project Settings > Plugins** to toggle, since a
   GDExtension isn't a scripted editor plugin in that sense.

You should now be able to add a `Terrain3D` node from the Create Node dialog. Continue to
[Quick start](02-quick-start.md).

## Build from source

Building from source is only necessary if you want to modify the plugin itself, or need a
platform/architecture the release archive doesn't cover.

### Build tools setup

Setting up TerrainServer for development is a standard GDExtension setup. This section gives a
quick overview for the main platforms. If you need a more in-depth guide, refer to the
[official Godot compiling documentation](https://docs.godotengine.org/en/stable/engine_details/development/compiling/index.html),
which is incredibly well explained and detailed.

You need Python, [SCons](https://scons.org/), a C++ compiler, and Git.

<details>
<summary><b>Linux</b></summary>

```sh
# Arch
pacman -Sy --noconfirm --needed \
  scons \
  pkgconf \
  gcc \
  libxcursor \
  libxinerama \
  libxi \
  libxrandr \
  wayland-utils \
  mesa \
  glu \
  libglvnd \
  alsa-lib \
  pulseaudio

# Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  scons \
  pkg-config \
  libx11-dev \
  libxcursor-dev \
  libxinerama-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libasound2-dev \
  libpulse-dev \
  libudev-dev \
  libxi-dev \
  libxrandr-dev \
  libwayland-dev

# Fedora
sudo dnf install -y \
  scons \
  pkgconfig \
  gcc-c++ \
  libstdc++-static \
  wayland-devel
```

Take a look at the [Godot Compiling for Linux, *BSD](https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_for_linuxbsd.html) for your distribution if you need more details.

</details>

<details>
<summary><b>Windows</b></summary>

Using Visual Studio (recommended):

1. Install [Visual Studio Community](https://visualstudio.microsoft.com/vs/community/) with the
   **Desktop development with C++** workload.
2. Install [Python 3.9+](https://www.python.org/downloads/windows/), enabling "Add python.exe to
   PATH" during setup.
3. Install SCons and Git:

```powershell
pip install scons
winget install Git.Git
```

4. Build from the **Developer Command Prompt for VS** (or **x64 Native Tools Command Prompt**),
   so the MSVC compiler is on `PATH`.

Other ways to compile for Windows: [MinGW-w64](https://www.mingw-w64.org/) (GCC),
[MinGW-LLVM](https://github.com/mstorsjo/llvm-mingw/releases) (clang), Scoop, or MSYS2. See the
[Godot Compiling for Windows](https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_for_windows.html)
guide for those.

</details>

<details>
<summary><b>macOS</b></summary>

```sh
# Homebrew (also fetches Command Line Tools for Xcode automatically)
brew install scons git

# or MacPorts
sudo port install scons
```

The [Vulkan SDK](https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_for_macos.html)
step in the official guide is only needed when building the full Godot engine, not a
GDExtension, skip it here.

Take a look at the [Godot Compiling for macOS](https://docs.godotengine.org/en/stable/engine_details/development/compiling/compiling_for_macos.html)
guide if you need more details.

</details>

Verify with `scons --version` and `git --version` before continuing.

```sh
git clone --recurse-submodules https://github.com/mdoradom/TerrainServer.git
cd TerrainServer
scons platform=linux target=template_debug   # debug build
scons platform=linux target=template_release # release build
```

`platform` is one of `linux`, `windows`, `macos`; `arch` defaults per-platform. If you
already have the repository cloned without submodules, fetch `godot-cpp` (pinned to its `4.5`
branch) with:

```sh
git submodule update --init --recursive
```

The build output lands in `demo/addons/terrain_server/bin/` as
`libterrain_server.<platform>.<target>[.<arch>].<ext>`, and
`demo/addons/terrain_server/terrain_server.gdextension` maps each platform/target/arch
combination to the matching file. To use a from-source build in your own project, copy the
whole `demo/addons/terrain_server/` directory (including the `.gdextension` file and your
rebuilt `bin/`) into your project's `res://addons/`.

**Never pass `dev_build=yes`.** It appends `.dev` to the output filename (for example,
`libterrain_server.linux.template_debug.dev.x86_64.so`), but no entry in
`terrain_server.gdextension` names that path, so Godot silently keeps loading whatever
non-`dev` binary is already on disk instead. There's no error: the build succeeds and simply has
no effect, which makes it easy to mistake a stale build for a fresh one. If you need a
debuggable build, use `scons platform=linux target=template_debug debug_symbols=yes
optimize=none` instead; that flag combination doesn't change the output filename.

# Terrain Server vX

Procedural terrain generation system for Godot

## Installation
1. Download \`${{ env.PLUGIN_NAME }}-v${{ steps.version.outputs.VERSION }}.zip\`
2. Extract to your project root (creates \`res://addons/terrain_server/\`)
3. Restart Godot
4. Enable in **Project > Project Settings > Plugins**

## Quick Start
1. Add a \`Terrain3D\` node to your scene
2. Create \`TerrainConfiguration\` resource in Inspector
3. Add \`FastNoiseLite\` noise
4. Adjust parameters (resolution, size, height)

## Features
- Real-time procedural generation with FastNoiseLite
- Configurable mesh resolution (1-512 vertices)
- Terrain size control (1-10000 units)
- Height scale adjustment (0.1-1000)
- Material override support
- Proper UV mapping

## Platform Support
- ✅ Linux x86_64
- ✅ Windows x86_64
- ✅ macOS (Universal)

## Requirements
- Godot 4.2+ (tested on 4.6)

## Support
Report issues: https://github.com/mdoradom/TerrainServer/issues
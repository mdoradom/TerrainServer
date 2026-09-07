# Terrain Server vX

Procedural terrain generation system for Godot

## Installation
1. Download the latest release archive (for example, \`terrain_server-v1.2.3.zip\`) from the GitHub Releases page
2. Extract to your project root (creates \`res://addons/terrain_server/\`)
3. Restart Godot, the extension loads automatically, no Plugins-tab toggle needed

## Quick Start
1. Add a \`Terrain3D\` node to your scene
2. Create a \`TerrainConfiguration\` resource and assign it to the node's \`configuration\` property
3. Adjust its parameters: height scale, mesh resolution, terrain size, noise octaves/frequency/lacunarity/gain, clipmap level count, albedo texture
4. Add \`TerrainBiomeLayer\` / \`TerrainSlopeLayer\` resources to shade by climate and slope

## Features
- Real-time procedural generation driven by \`TerrainConfiguration\`
- Clipmap-based level-of-detail rendering
- Physics collision that tracks the rendered surface
- Biome (temperature/moisture) and slope-based layering
- Editor preview: live in-viewport terrain, debug gizmos, and a bottom-panel dock with stats, a Whittaker climate chart and a live probe

## Platform Support
- ✅ Linux x86_64
- ✅ Windows x86_64
- ✅ macOS (Universal)

## Requirements
- Godot 4.2+ (tested on 4.6)

## Support
Report issues: https://github.com/mdoradom/TerrainServer/issues
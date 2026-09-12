#!/usr/bin/env bash
# Regenerates the editor's preview audio from the master track.
#
# Godot 4.7 ships no FLAC loader -- ResourceLoader has no loader registered for it, so the master in
# demo/trailer/audio/*.flac cannot be played back in-engine at all. The editor plays this Ogg Vorbis
# transcode instead (trailer_music.gd), at preview quality: it is only ever used to hear whether a
# cut lands on a beat, never mixed into anything.
#
# The cue and waveform data are extracted from the *master*, not from this file:
#   python demo/trailer/tools/extract_cues.py  "demo/trailer/audio/klsr - Bliss.flac" --bridge-hint 67
#   python demo/trailer/tools/extract_peaks.py "demo/trailer/audio/klsr - Bliss.flac"
#
# Usage: demo/trailer/tools/make_preview_audio.sh [input-audio]
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
audio_dir="$(dirname "$here")/audio"
input="${1:-$audio_dir/klsr - Bliss.flac}"
output="$audio_dir/bliss_preview.ogg"

if [[ ! -f "$input" ]]; then
	echo "no master audio at '$input'" >&2
	exit 1
fi

# -q:a 3 lands around 110kbps: indistinguishable from the master for judging timing, and small
# enough to live in the repo beside it.
ffmpeg -v error -y -i "$input" -c:a libvorbis -q:a 3 -ac 2 "$output"

echo "wrote $output ($(du -h "$output" | cut -f1))"

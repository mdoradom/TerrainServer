#!/usr/bin/env python3
"""Extract a waveform envelope for the trailer editor's timeline ruler.

The editor draws the track under the playhead so a value can be dialled in against the
shape of the music, not just against cue ticks. That only needs an envelope, so this
decodes to mono PCM with ffmpeg (same approach as extract_cues.py's fallback path) and
reduces it to one peak + one RMS value per frame at the trailer's 60fps -- small enough
to ship as JSON and to index straight by frame number at draw time.
"""

import argparse
import json
import subprocess
from pathlib import Path

import numpy as np

SAMPLE_RATE = 22050


def _decode_to_mono_pcm(audio_path: Path, sample_rate: int) -> np.ndarray:
    cmd = [
        "ffmpeg", "-v", "error", "-i", str(audio_path),
        "-f", "f32le", "-ac", "1", "-ar", str(sample_rate), "-",
    ]
    raw = subprocess.run(cmd, check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audio", type=Path, help="Path to the trailer audio file")
    parser.add_argument("--out", type=Path, default=Path("demo/trailer/audio_peaks.json"))
    parser.add_argument("--fps", type=float, default=60.0, help="Bins per second (match the trailer's fps)")
    args = parser.parse_args()

    y = _decode_to_mono_pcm(args.audio, SAMPLE_RATE)
    duration = len(y) / SAMPLE_RATE

    # One bin per frame, trimmed to whole bins so bin i always covers frame i exactly.
    per_bin = int(round(SAMPLE_RATE / args.fps))
    bins = len(y) // per_bin
    framed = y[: bins * per_bin].reshape(bins, per_bin)

    peak = np.abs(framed).max(axis=1)
    rms = np.sqrt((framed.astype(np.float64) ** 2).mean(axis=1))

    # Normalised, so the ruler draws the same height regardless of the master's loudness.
    peak_scale = float(peak.max()) or 1.0
    rms_scale = float(rms.max()) or 1.0

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps({
        "source": args.audio.name,
        "duration_sec": round(duration, 3),
        "fps": args.fps,
        "peak": [round(float(v) / peak_scale, 4) for v in peak],
        "rms": [round(float(v) / rms_scale, 4) for v in rms],
    }) + "\n")
    print(f"wrote {args.out}: {bins} bins at {args.fps}fps, duration={duration:.3f}s")


if __name__ == "__main__":
    main()

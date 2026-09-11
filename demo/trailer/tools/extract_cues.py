#!/usr/bin/env python3
"""Extract audio cue timestamps that drive the trailer clips' cut/shot timing.

Tries librosa first (pip install librosa) for its onset-strength envelope; falls back to
a self-written ffmpeg + numpy spectral-flux detector if librosa isn't importable. Both
paths hand the same continuous strength envelope to one shared peak-picker below, so cue
density and the definition of "strong" stay identical regardless of which one ran, and
trailer_rig.gd's loader never needs to know which one produced audio_cues.json.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

import numpy as np


def _decode_to_mono_pcm(audio_path: Path, sample_rate: int) -> np.ndarray:
    cmd = [
        "ffmpeg", "-v", "error", "-i", str(audio_path),
        "-f", "f32le", "-ac", "1", "-ar", str(sample_rate), "-",
    ]
    raw = subprocess.run(cmd, check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32)


def _spectral_flux(y: np.ndarray, sr: int, hop: int = 512, n_fft: int = 2048):
    """Half-wave-rectified frame-to-frame spectral difference: a novelty envelope."""
    window = np.hanning(n_fft)
    n_frames = 1 + (len(y) - n_fft) // hop
    mags = np.empty((n_frames, n_fft // 2 + 1), dtype=np.float32)
    for i in range(n_frames):
        frame = y[i * hop : i * hop + n_fft] * window
        mags[i] = np.abs(np.fft.rfft(frame))
    flux = np.sum(np.maximum(0.0, np.diff(mags, axis=0)), axis=1)
    flux = np.concatenate([[0.0], flux])
    times = np.arange(n_frames) * hop / sr
    return times, flux


def _pick_peaks(times: np.ndarray, strength: np.ndarray, min_gap_sec: float, threshold_k: float):
    """Local maxima above mean + threshold_k*std, at least min_gap_sec apart. Tune
    threshold_k/min_gap_sec to trade "every hit" density against "only the marked ones"."""
    threshold = strength.mean() + threshold_k * strength.std()
    peaks = []
    last_t = -min_gap_sec
    for i in range(1, len(strength) - 1):
        if strength[i] < threshold:
            continue
        if not (strength[i] > strength[i - 1] and strength[i] >= strength[i + 1]):
            continue
        if times[i] - last_t < min_gap_sec:
            continue
        peaks.append((float(times[i]), float(strength[i])))
        last_t = times[i]
    return peaks


def _select_strong(peaks, min_gap_sec: float, keep_fraction: float):
    """Greedily keep the loudest peaks first, skipping any within min_gap_sec of one
    already kept, until roughly keep_fraction of all peaks survive — the "sonidos muy
    marcados" subset meant for the big visual cuts, versus every detected onset."""
    target = max(1, int(round(len(peaks) * keep_fraction)))
    kept: list[float] = []
    for t, _s in sorted(peaks, key=lambda p: -p[1]):
        if all(abs(t - kt) >= min_gap_sec for kt in kept):
            kept.append(t)
        if len(kept) >= target:
            break
    return {round(t, 3) for t in kept}


def _detect_with_librosa(audio_path: Path):
    import librosa

    y, sr = librosa.load(str(audio_path), sr=44100, mono=True)
    onset_env = librosa.onset.onset_strength(y=y, sr=sr)
    times = librosa.frames_to_time(np.arange(len(onset_env)), sr=sr)
    rms = librosa.feature.rms(y=y)[0]
    rms_times = librosa.frames_to_time(np.arange(len(rms)), sr=sr)
    duration = librosa.get_duration(y=y, sr=sr)
    return times, onset_env, rms_times, rms, duration, "librosa"


def _detect_with_fallback(audio_path: Path):
    sr = 22050
    y = _decode_to_mono_pcm(audio_path, sr)
    times, flux = _spectral_flux(y, sr)
    hop, frame_len = 512, 2048
    n_frames = 1 + (len(y) - frame_len) // hop
    rms = np.array([np.sqrt(np.mean(y[i * hop : i * hop + frame_len] ** 2)) for i in range(n_frames)])
    rms_times = np.arange(n_frames) * hop / sr
    duration = len(y) / sr
    return times, flux, rms_times, rms, duration, "fallback"


def _find_bridge(peak_times, peak_strengths, rms_times, rms, hint: float | None,
                  search_start=45.0, search_end=90.0, drop_window=15.0):
    """With a --bridge-hint (an approximate second the user places by ear), just snap to
    the nearest detected peak — the exact automatic guess doesn't need to be perfect since
    the user fine-tunes the real cut with a fade in their own edit anyway. Without a hint,
    fall back to a blind heuristic: the quietest stretch in the search window is the
    breakdown, and the loudest peak in the following drop_window seconds is the drop back
    into the second part."""
    if hint is not None:
        if len(peak_times) == 0:
            return float(hint)
        return float(peak_times[np.argmin(np.abs(peak_times - hint))])
    mask = (rms_times >= search_start) & (rms_times <= search_end)
    if not mask.any():
        return None
    quiet_time = rms_times[mask][np.argmin(rms[mask])]
    after = (peak_times >= quiet_time) & (peak_times <= quiet_time + drop_window)
    if not after.any():
        return float(quiet_time)
    candidates, candidate_strengths = peak_times[after], peak_strengths[after]
    return float(candidates[np.argmax(candidate_strengths)])


def _write_plot(path: Path, rms_times, rms, cues, method):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available, skipping plot", file=sys.stderr)
        return
    fig, ax = plt.subplots(figsize=(14, 4))
    ax.plot(rms_times, rms / (rms.max() or 1.0), color="steelblue", alpha=0.5, label="rms")
    colors = {"onset": "gray", "strong": "orange", "bridge": "red"}
    for c in cues:
        ax.axvline(c["time_sec"], color=colors[c["kind"]], alpha=0.6, linewidth=2 if c["kind"] == "bridge" else 1)
    ax.set_xlabel("time (s)")
    ax.set_title(f"cues ({method}) — red = bridge, orange = strong, gray = onset")
    fig.tight_layout()
    fig.savefig(path, dpi=120)
    print(f"wrote {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("audio", type=Path, help="Path to the trailer audio file")
    parser.add_argument("--out", type=Path, default=Path("demo/trailer/audio_cues.json"))
    parser.add_argument("--plot", type=Path, default=None, help="Optional PNG path for a novelty-curve sanity check")
    parser.add_argument("--min-gap", type=float, default=0.25, help="Minimum seconds between any two cues")
    parser.add_argument("--threshold-k", type=float, default=2.0, help="Peak threshold in std-devs above the mean")
    parser.add_argument("--strong-min-gap", type=float, default=1.0, help="Minimum seconds between 'strong' cues")
    parser.add_argument("--strong-fraction", type=float, default=0.25, help="Fraction of all cues kept as 'strong'")
    parser.add_argument("--bridge-hint", type=float, default=None, help="Approximate second (by ear) of the bridge; snaps to the nearest detected peak")
    args = parser.parse_args()

    try:
        times, strength, rms_times, rms, duration, method = _detect_with_librosa(args.audio)
    except ImportError:
        print("librosa not available, falling back to the ffmpeg+numpy spectral-flux detector", file=sys.stderr)
        times, strength, rms_times, rms, duration, method = _detect_with_fallback(args.audio)

    peaks = _pick_peaks(times, strength, min_gap_sec=args.min_gap, threshold_k=args.threshold_k)
    peak_times = np.array([t for t, _ in peaks])
    peak_strengths = np.array([s for _, s in peaks])

    strong_set = _select_strong(peaks, min_gap_sec=args.strong_min_gap, keep_fraction=args.strong_fraction)
    bridge_time = _find_bridge(peak_times, peak_strengths, rms_times, rms, args.bridge_hint) if len(peak_times) else None

    cues = [
        {"time_sec": round(t, 3), "kind": ("strong" if round(t, 3) in strong_set else "onset"), "strength": round(s, 4)}
        for t, s in peaks
    ]
    if bridge_time is not None and cues:
        min(cues, key=lambda c: abs(c["time_sec"] - bridge_time))["kind"] = "bridge"

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(
            {"source": args.audio.name, "method": method, "duration_sec": round(float(duration), 3), "cues": cues},
            indent=2,
        )
    )

    bridge = next((c for c in cues if c["kind"] == "bridge"), None)
    strong_count = sum(1 for c in cues if c["kind"] == "strong")
    print(f"method={method} duration={duration:.2f}s cues={len(cues)} strong={strong_count}")
    if bridge:
        print(f"bridge candidate at {bridge['time_sec']:.2f}s (verify by ear)")

    if args.plot:
        _write_plot(args.plot, rms_times, rms, cues, method)


if __name__ == "__main__":
    main()

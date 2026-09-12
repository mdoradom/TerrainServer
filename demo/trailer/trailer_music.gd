extends AudioStreamPlayer

# Preview-only music playback for the trailer editor (see TODO_TRAILER.md T10).
#
# The trailer is cut to "Bliss", and every authored moment in trailer_timeline.json is a timestamp
# on that track. Until now those timestamps could only be checked against the cue list and the
# waveform; this plays the track under the preview so a beat can be judged by ear, which is the
# only way to tell whether a cut lands on a hit or a frame beside it.
#
# Nothing here ever reaches a recording. The rig only builds this node on the --controls path, which
# already refuses to run during --write-movie, and the recorded clips stay silent by design: the
# user mixes the real audio in their own edit (see TODO_TRAILER.md's "Out of scope").
#
# Two details make the sync work:
#
# * The shot's time axis IS the track's time axis -- the build shot spans 0s-63.936s of the song --
#   so a playhead position is a stream position, with no offset to map between them.
# * While playing, the *audio* is the clock and the visuals follow it. Accumulating `delta` on the
#   visual side instead would drift against the sound card's own rate within seconds, which is
#   exactly the error this is meant to reveal rather than introduce.

# Godot 4.7 ships no FLAC loader, so the editor plays an Ogg Vorbis transcode of the master rather
# than demo/trailer/audio/*.flac. tools/make_preview_audio.sh regenerates it.
const AUDIO_PATH := "res://trailer/audio/bliss_preview.ogg"

# Re-seek only when the drift is audible; correcting every frame would fight the mixer and chatter.
const RESYNC_THRESHOLD := 0.22

var _available := false
var _enabled := true


func setup() -> bool:
	# load_from_file() reads the file directly rather than going through the import pipeline, so the
	# preview needs no .import sidecar (which is gitignored anyway) and no editor import pass.
	var stream := AudioStreamOggVorbis.load_from_file(AUDIO_PATH)
	if stream == null:
		stream = AudioStreamOggVorbis.load_from_file(ProjectSettings.globalize_path(AUDIO_PATH))
	if stream == null:
		printerr("[trailer] no preview audio at %s; run tools/make_preview_audio.sh" % AUDIO_PATH)
		return false

	# Looping is the rig's job: it owns the loop region and has to restart the visuals at the same
	# instant, so the stream must not wrap on its own.
	stream.loop = false
	self.stream = stream
	_available = true
	return true


func is_available() -> bool:
	return _available


func is_enabled() -> bool:
	return _available and _enabled


func set_enabled(p_enabled: bool) -> void:
	_enabled = p_enabled
	if not _enabled:
		stop()


# The audio clock, in track seconds. get_playback_position() only advances per mixed buffer, so the
# two AudioServer terms interpolate within the current one and take the output latency off -- without
# them the playhead visibly stair-steps at 60fps against a smoothly moving image.
func clock() -> float:
	if not playing:
		return -1.0
	return get_playback_position() + AudioServer.get_time_since_last_mix() - AudioServer.get_output_latency()


func start_at(p_time: float) -> void:
	if not is_enabled():
		return
	if p_time < 0.0 or p_time >= stream.get_length():
		stop()
		return
	play(p_time)


func stop_music() -> void:
	if playing:
		stop()


# Called every frame by the rig with the moment it wants heard. Returns the time the visuals should
# actually show: the audio clock while it is the master, or p_time unchanged when music is off, the
# preview is paused, or the playhead has been moved somewhere the stream cannot follow.
func sync(p_time: float, p_playing: bool, p_speed: float) -> float:
	if not is_enabled():
		stop_music()
		return p_time

	if not p_playing:
		stop_music()
		return p_time

	# Preview speed retunes the track with it; a scrub at 0.25x is still recognisable enough to
	# place a hit by, and keeping pitch would need a time stretcher this does not warrant.
	pitch_scale = p_speed

	if not playing:
		start_at(p_time)
		return p_time

	var position := clock()
	if position < 0.0:
		return p_time

	# A jump (a seek, a loop wrap, a cue step) shows up as the stream being somewhere else entirely;
	# follow the rig back rather than dragging the picture to where the sound happens to be.
	if absf(position - p_time) > RESYNC_THRESHOLD:
		start_at(p_time)
		return p_time

	return position

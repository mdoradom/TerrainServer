extends Control

# The trailer editor's timeline ruler: the full-width strip along the bottom of the --controls
# layout that shows where you are in the track (see TODO_TRAILER.md T9).
#
# Four lanes, stacked: the chapters of the build-up, the waveform of the music, the detected audio
# cues, and a second ruler. Tuning this trailer means landing authored values on musical hits, and
# that is a lot easier when the hits are visible as a shape rather than as a list of timestamps --
# so the waveform is the backdrop the cue ticks and chapter boundaries are read against.
#
# Drawing is split across two child Controls on purpose. The lanes are expensive (one line per pixel
# column of waveform) but only change when the shot or the size does; the playhead changes every
# frame. Redrawing only the overlay per frame keeps a 60fps preview from spending its budget
# rasterising a waveform that did not move.

signal seek_requested(time: float)
signal loop_changed(from: float, to: float)

const LANE_CHAPTERS := 19.0
const LANE_WAVE := 54.0
const LANE_CUES := 11.0
const LANE_TICKS := 15.0
const LANE_GAP := 3.0
const HEIGHT := LANE_CHAPTERS + LANE_WAVE + LANE_CUES + LANE_TICKS + LANE_GAP * 3.0

const ACCENT := Color(0.30, 0.82, 1.0)
const WAVE_COLOR := Color(0.22, 0.52, 0.70)
const WAVE_RMS_COLOR := Color(0.40, 0.86, 1.0)
const PLAYHEAD_COLOR := Color(1.0, 0.86, 0.35)
const BRIDGE_COLOR := Color(1.0, 0.45, 0.35)
const LOOP_COLOR := Color(0.55, 1.0, 0.70)
const TEXT_COLOR := Color(0.72, 0.92, 1.0)

# Cue kinds, matching trailer_rig.gd's get_cue_kinds().
const KIND_ONSET := 0
const KIND_STRONG := 1
const KIND_BRIDGE := 2

var _rig: Node3D
var _font: Font

var _start := 0.0
var _end := 1.0
var _time := 0.0

var _cue_times := PackedFloat32Array()
var _cue_kinds := PackedByteArray()
var _peaks := PackedFloat32Array()
var _peaks_rms := PackedFloat32Array()
var _peaks_fps := 60.0

var _loop_from := -1.0
var _loop_to := -1.0

var _static_layer: Control
var _overlay_layer: Control

var _dragging := false
var _loop_dragging := false
var _loop_anchor := 0.0
var _hover_time := -1.0


func setup(p_rig: Node3D) -> void:
	_rig = p_rig
	_font = ThemeDB.fallback_font

	var window: Vector2 = p_rig.get_shot_window()
	_start = window.x
	_end = maxf(window.y, window.x + 0.001)
	_time = _start

	_cue_times = p_rig.get_cue_times()
	_cue_kinds = p_rig.get_cue_kinds()
	_peaks = p_rig.get_peaks()
	_peaks_rms = p_rig.get_peaks_rms()
	_peaks_fps = p_rig.get_peaks_fps()

	custom_minimum_size.y = HEIGHT
	mouse_filter = Control.MOUSE_FILTER_STOP

	_static_layer = _add_layer(_draw_lanes)
	_overlay_layer = _add_layer(_draw_overlay)
	resized.connect(func(): _static_layer.queue_redraw())


func set_time(p_time: float) -> void:
	if is_equal_approx(p_time, _time):
		return
	_time = p_time
	_overlay_layer.queue_redraw()


func set_loop(p_from: float, p_to: float) -> void:
	_loop_from = p_from
	_loop_to = p_to
	_overlay_layer.queue_redraw()


func has_loop() -> bool:
	return _loop_to > _loop_from


# Redraws the lanes themselves. Call after anything that moves a chapter boundary, since the
# chapter band is authored data the panel can edit live.
func refresh_lanes() -> void:
	if _static_layer != null:
		_static_layer.queue_redraw()


func _add_layer(p_draw: Callable) -> Control:
	var layer := Control.new()
	layer.set_anchors_preset(Control.PRESET_FULL_RECT)
	layer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	layer.draw.connect(p_draw.bind(layer))
	add_child(layer)
	return layer


# --- Geometry -------------------------------------------------------------------------------

func _lanes() -> Dictionary:
	var w := size.x
	var y := 0.0
	var chapters := Rect2(0.0, y, w, LANE_CHAPTERS)
	y += LANE_CHAPTERS + LANE_GAP
	var wave := Rect2(0.0, y, w, LANE_WAVE)
	y += LANE_WAVE + LANE_GAP
	var cues := Rect2(0.0, y, w, LANE_CUES)
	y += LANE_CUES + LANE_GAP
	var ticks := Rect2(0.0, y, w, LANE_TICKS)
	return {"chapters": chapters, "wave": wave, "cues": cues, "ticks": ticks}


func _time_to_x(p_time: float) -> float:
	return (p_time - _start) / (_end - _start) * size.x


func _x_to_time(p_x: float) -> float:
	return clampf(_start + p_x / maxf(size.x, 1.0) * (_end - _start), _start, _end)


func _nearest_cue(p_time: float) -> float:
	var best := p_time
	var best_distance := INF
	for cue in _cue_times:
		var distance := absf(cue - p_time)
		if distance < best_distance:
			best_distance = distance
			best = cue
	return best


# --- Lane drawing ---------------------------------------------------------------------------

func _draw_lanes(p_layer: Control) -> void:
	var lanes := _lanes()
	_draw_chapter_lane(p_layer, lanes["chapters"])
	_draw_wave_lane(p_layer, lanes["wave"])
	_draw_cue_lane(p_layer, lanes["cues"])
	_draw_tick_lane(p_layer, lanes["ticks"])


func _draw_chapter_lane(p_layer: Control, p_rect: Rect2) -> void:
	var chapters: Array = _rig.timeline["chapters"]
	for i in chapters.size():
		var from := float(chapters[i]["start"])
		var to := _end if i == chapters.size() - 1 else float(chapters[i + 1]["start"])
		if to <= _start or from >= _end:
			continue

		var x0 := _time_to_x(maxf(from, _start))
		var x1 := _time_to_x(minf(to, _end))
		var band := Rect2(x0, p_rect.position.y, maxf(x1 - x0, 1.0), p_rect.size.y)
		# Alternating tints alone read as two chapters, not six, so the hue walks as well.
		var hue := fmod(0.52 + float(i) * 0.075, 1.0)
		p_layer.draw_rect(band, Color.from_hsv(hue, 0.55, 0.42, 0.55))
		p_layer.draw_line(Vector2(x0, p_rect.position.y), Vector2(x0, p_rect.end.y),
				Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.75), 1.0)

		var name := str(chapters[i]["name"]).to_upper()
		if band.size.x > 42.0:
			p_layer.draw_string(_font, Vector2(x0 + 5.0, p_rect.end.y - 5.0), name,
					HORIZONTAL_ALIGNMENT_LEFT, band.size.x - 8.0, 11, TEXT_COLOR)


func _draw_wave_lane(p_layer: Control, p_rect: Rect2) -> void:
	p_layer.draw_rect(p_rect, Color(0.04, 0.07, 0.10, 0.85))
	var mid := p_rect.position.y + p_rect.size.y * 0.5
	var half := p_rect.size.y * 0.5 - 1.0

	if _peaks.is_empty():
		p_layer.draw_string(_font, Vector2(8.0, mid + 4.0),
				"no audio_peaks.json -- run tools/extract_peaks.py",
				HORIZONTAL_ALIGNMENT_LEFT, -1, 11, Color(0.55, 0.62, 0.70))
		return

	# One column per pixel, each taking the loudest bin it covers: at 60 bins/second and a shot a
	# minute long there are about two bins per column, and taking the max keeps single-frame
	# transients visible instead of averaging them away.
	var columns := int(p_rect.size.x)
	for column in columns:
		var t0 := _x_to_time(float(column))
		var t1 := _x_to_time(float(column + 1))
		var i0 := clampi(int(t0 * _peaks_fps), 0, _peaks.size() - 1)
		var i1 := clampi(int(t1 * _peaks_fps), i0, _peaks.size() - 1)

		var peak := 0.0
		var rms := 0.0
		for i in range(i0, i1 + 1):
			peak = maxf(peak, _peaks[i])
			if i < _peaks_rms.size():
				rms = maxf(rms, _peaks_rms[i])

		var x := float(column) + 0.5
		if peak > 0.0:
			p_layer.draw_line(Vector2(x, mid - peak * half), Vector2(x, mid + peak * half),
					Color(WAVE_COLOR.r, WAVE_COLOR.g, WAVE_COLOR.b, 0.9), 1.0)
		if rms > 0.0:
			p_layer.draw_line(Vector2(x, mid - rms * half * 0.82), Vector2(x, mid + rms * half * 0.82),
					Color(WAVE_RMS_COLOR.r, WAVE_RMS_COLOR.g, WAVE_RMS_COLOR.b, 0.55), 1.0)


func _draw_cue_lane(p_layer: Control, p_rect: Rect2) -> void:
	for i in _cue_times.size():
		var t := _cue_times[i]
		if t < _start or t > _end:
			continue
		var kind := _cue_kinds[i] if i < _cue_kinds.size() else KIND_ONSET
		var x := _time_to_x(t)

		var color := Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.55)
		var height := p_rect.size.y * 0.55
		var width := 1.0
		if kind == KIND_STRONG:
			color = Color(ACCENT.r, ACCENT.g, ACCENT.b, 0.95)
			height = p_rect.size.y
			width = 1.5
		elif kind == KIND_BRIDGE:
			color = BRIDGE_COLOR
			height = p_rect.size.y
			width = 2.5

		p_layer.draw_line(Vector2(x, p_rect.end.y - height), Vector2(x, p_rect.end.y), color, width)


func _draw_tick_lane(p_layer: Control, p_rect: Rect2) -> void:
	# Coarse enough that the labels never collide at the narrowest usable panel width.
	var span := _end - _start
	var step := 1.0
	while span / step > 70.0:
		step *= 5.0

	var t := ceilf(_start / step) * step
	while t <= _end:
		var x := _time_to_x(t)
		var labelled := fmod(t, step * 5.0) < 0.001 or step >= 5.0
		p_layer.draw_line(Vector2(x, p_rect.position.y),
				Vector2(x, p_rect.position.y + (6.0 if labelled else 3.0)),
				Color(TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b, 0.5), 1.0)
		if labelled:
			p_layer.draw_string(_font, Vector2(x + 3.0, p_rect.end.y - 2.0), "%ds" % int(round(t)),
					HORIZONTAL_ALIGNMENT_LEFT, -1, 10, Color(TEXT_COLOR.r, TEXT_COLOR.g, TEXT_COLOR.b, 0.7))
		t += step


# --- Overlay drawing ------------------------------------------------------------------------

func _draw_overlay(p_layer: Control) -> void:
	var lanes := _lanes()
	var full := Rect2(0.0, 0.0, size.x, size.y)

	# Everything outside the loop region is dimmed, so a region set for tuning one beat is
	# unmistakable without hiding what surrounds it.
	if has_loop():
		var x0 := _time_to_x(_loop_from)
		var x1 := _time_to_x(_loop_to)
		p_layer.draw_rect(Rect2(0.0, 0.0, x0, size.y), Color(0.0, 0.0, 0.0, 0.55))
		p_layer.draw_rect(Rect2(x1, 0.0, size.x - x1, size.y), Color(0.0, 0.0, 0.0, 0.55))
		for x in [x0, x1]:
			p_layer.draw_line(Vector2(x, 0.0), Vector2(x, size.y), LOOP_COLOR, 1.5)

	# The chapter on screen, outlined in its own lane.
	var chapters: Array = _rig.timeline["chapters"]
	var index: int = _rig.get_chapter_index(_time)
	if index >= 0 and index < chapters.size():
		var from := float(chapters[index]["start"])
		var to := _end if index == chapters.size() - 1 else float(chapters[index + 1]["start"])
		var x0 := _time_to_x(maxf(from, _start))
		var x1 := _time_to_x(minf(to, _end))
		p_layer.draw_rect(Rect2(x0, lanes["chapters"].position.y, maxf(x1 - x0, 1.0),
				lanes["chapters"].size.y), Color(1.0, 1.0, 1.0, 0.95), false, 1.5)

	if _hover_time >= 0.0:
		var hx := _time_to_x(_hover_time)
		p_layer.draw_line(Vector2(hx, 0.0), Vector2(hx, size.y), Color(1.0, 1.0, 1.0, 0.22), 1.0)

	var x := _time_to_x(_time)
	p_layer.draw_line(Vector2(x, 0.0), Vector2(x, size.y), PLAYHEAD_COLOR, 1.5)
	p_layer.draw_rect(Rect2(x - 4.0, 0.0, 8.0, 6.0), PLAYHEAD_COLOR)
	p_layer.draw_string(_font, Vector2(clampf(x + 6.0, 0.0, size.x - 52.0), full.size.y - 3.0),
			"%.3f" % _time, HORIZONTAL_ALIGNMENT_LEFT, -1, 11, PLAYHEAD_COLOR)


# --- Interaction ----------------------------------------------------------------------------

func _gui_input(p_event: InputEvent) -> void:
	if p_event is InputEventMouseButton:
		var button := p_event as InputEventMouseButton
		match button.button_index:
			MOUSE_BUTTON_LEFT:
				# Alt reuses the left button for the loop region, for trackpads with no right drag.
				if button.pressed and button.alt_pressed:
					_loop_dragging = true
					_loop_anchor = _x_to_time(button.position.x)
					set_loop(_loop_anchor, _loop_anchor)
				elif button.pressed:
					_dragging = true
					_seek_to(button.position.x, button.ctrl_pressed)
				else:
					_dragging = false
					_commit_loop()
			MOUSE_BUTTON_RIGHT:
				if button.pressed:
					_loop_dragging = true
					_loop_anchor = _x_to_time(button.position.x)
					set_loop(_loop_anchor, _loop_anchor)
				else:
					_commit_loop()
			MOUSE_BUTTON_MIDDLE:
				if button.pressed:
					set_loop(-1.0, -1.0)
					loop_changed.emit(-1.0, -1.0)

	elif p_event is InputEventMouseMotion:
		var motion := p_event as InputEventMouseMotion
		_hover_time = _x_to_time(motion.position.x)
		if _loop_dragging:
			var t := _x_to_time(motion.position.x)
			set_loop(minf(_loop_anchor, t), maxf(_loop_anchor, t))
		elif _dragging:
			_seek_to(motion.position.x, motion.ctrl_pressed)
		_overlay_layer.queue_redraw()


func _notification(p_what: int) -> void:
	if p_what == NOTIFICATION_MOUSE_EXIT and _overlay_layer != null:
		_hover_time = -1.0
		_overlay_layer.queue_redraw()


func _seek_to(p_x: float, p_snap: bool) -> void:
	var t := _x_to_time(p_x)
	if p_snap:
		t = _nearest_cue(t)
	seek_requested.emit(t)


# A region shorter than a couple of frames is a mis-click, not an intended loop.
func _commit_loop() -> void:
	_loop_dragging = false
	if _loop_to - _loop_from < 2.0 / 60.0:
		set_loop(-1.0, -1.0)
		loop_changed.emit(-1.0, -1.0)
		return
	loop_changed.emit(_loop_from, _loop_to)

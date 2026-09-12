extends CanvasLayer

# Live tuning panel for the trailer's build-up shot, enabled with `--controls` (see
# TODO_TRAILER.md). It edits trailer_timeline.json in memory and writes it back on Save, so what
# you dial in by eye is exactly what the next --write-movie records.
#
# The panel is never up during a recording -- trailer_rig.gd refuses to enable it when
# --write-movie is present, because it draws into the same viewport the movie captures.
#
# Every slider is declared as a path into the timeline dictionary rather than wired by hand:
# GDScript dictionaries are references, so writing through the path mutates the live timeline and
# the rig re-applies the current frame immediately.

const PANEL_WIDTH := 430.0
const LABEL_WIDTH := 186.0
const VALUE_WIDTH := 62.0
const CUE_EPSILON := 0.02

# section -> [label, path, min, max, step]. "chapter" paths are resolved against whichever chapter
# is on screen, so the same rows follow the playhead from beat to beat.
const LOOK_ROWS := [
	["Wire intensity", ["look", "wire_intensity"], 0.0, 4.0, 0.01],
	["Wire dim", ["look", "wire_intensity_dim"], 0.0, 4.0, 0.01],
	["Wire width", ["look", "wire_width"], 0.2, 4.0, 0.01],
	["Wire opacity", ["look", "wire_opacity"], 0.0, 1.0, 0.01],
	["Diagonals", ["look", "diagonal_intensity"], 0.0, 1.0, 0.01],
	["Fill gain", ["look", "fill_gain"], 0.0, 2.0, 0.01],
	["Shade", ["look", "shade_amount"], 0.0, 2.0, 0.01],
	["Reveal edge", ["look", "reveal_edge"], 0.0, 400.0, 1.0],
	["Skirt depth", ["look", "skirt_depth"], 0.0, 800.0, 1.0],
	["Glow intensity", ["look", "glow_intensity"], 0.0, 3.0, 0.01],
	["Glow strength", ["look", "glow_strength"], 0.0, 2.0, 0.01],
	["Glow bloom", ["look", "glow_bloom"], 0.0, 1.0, 0.01],
	["Glow threshold", ["look", "glow_hdr_threshold"], 0.0, 4.0, 0.01],
]

const EFFECT_ROWS := [
	["Pulse decay", ["effects", "pulse_decay"], 0.01, 1.0, 0.005],
	["Pulse gain", ["effects", "pulse_gain"], 0.0, 8.0, 0.05],
	["Onset amplitude", ["effects", "pulse_onset_amplitude"], 0.0, 1.0, 0.01],
	["Ripple speed", ["effects", "ripple_speed"], 0.0, 4000.0, 10.0],
	["Ripple decay", ["effects", "ripple_decay"], 0.05, 2.0, 0.01],
	["Ripple lift", ["effects", "ripple_lift"], 0.0, 120.0, 0.5],
	["Ripple width", ["effects", "ripple_width"], 5.0, 400.0, 1.0],
	["Cut settle", ["effects", "cut_settle"], 0.0, 0.2, 0.001],
	["Settle time", ["effects", "cut_settle_duration"], 0.1, 8.0, 0.1],
	["FOV punch", ["effects", "fov_punch"], 0.0, 0.2, 0.001],
	["Light angle", ["effects", "light_base_angle"], 0.0, 360.0, 1.0],
	["Light rate", ["effects", "light_rate"], 0.0, 1.0, 0.01],
	["Flash time", ["effects", "bridge_flash_duration"], 0.0, 2.0, 0.01],
	["Flash wire", ["effects", "bridge_flash_wire"], 0.0, 16.0, 0.1],
	["Flash fill", ["effects", "bridge_flash_fill"], 0.0, 8.0, 0.1],
]

const CAMERA_ROWS := [
	["Yaw", ["chapter", "camera", "yaw"], -180.0, 180.0, 0.5],
	["Pitch", ["chapter", "camera", "pitch"], -89.0, 0.0, 0.5],
	["Distance", ["chapter", "camera", "distance"], 300.0, 6000.0, 10.0],
	["FOV", ["chapter", "camera", "fov"], 10.0, 90.0, 0.5],
	["Target height", ["chapter", "camera", "height"], -200.0, 800.0, 5.0],
	["Cut at", ["chapter", "start"], 0.0, 63.936, 0.001],
]

# Content rows are per chapter: only those whose keys the current chapter actually has are shown.
const CONTENT_ROWS := [
	["View A", ["chapter", "view_a"], 0.0, 6.0, 1.0],
	["View B", ["chapter", "view_b"], 0.0, 6.0, 1.0],
	["Fill", ["chapter", "fill_intensity"], 0.0, 4.0, 0.01],
	["Wire", ["chapter", "wire_intensity"], 0.0, 4.0, 0.01],
	["Shade", ["chapter", "shade"], 0.0, 2.0, 0.01],
	["Sweep time", ["chapter", "sweep_duration"], 0.05, 6.0, 0.05],
	["Level 0 draw", ["chapter", "level0_duration"], 0.1, 12.0, 0.1],
	["Ring draw", ["chapter", "ring_duration"], 0.05, 4.0, 0.05],
	["Lift time", ["chapter", "lift_duration"], 0.1, 6.0, 0.05],
	["Skirt at", ["chapter", "skirt_start"], 0.0, 63.936, 0.01],
	["Skirt time", ["chapter", "skirt_duration"], 0.1, 6.0, 0.05],
	["Ridge at", ["chapter", "ridge_time"], 0.0, 63.936, 0.01],
	["Warp at", ["chapter", "warp_time"], 0.0, 63.936, 0.01],
	["Continent at", ["chapter", "continent_time"], 0.0, 63.936, 0.01],
	["Redistrib at", ["chapter", "redistribution_time"], 0.0, 63.936, 0.01],
	["Lit at", ["chapter", "lit_time"], 0.0, 63.936, 0.01],
	["Lit time", ["chapter", "lit_duration"], 0.05, 6.0, 0.05],
	["Moisture at", ["chapter", "moisture_time"], 0.0, 63.936, 0.01],
	["Breath at", ["chapter", "breath_start"], 0.0, 63.936, 0.01],
	["Breath time", ["chapter", "breath_duration"], 0.1, 12.0, 0.1],
	["Reveal time", ["chapter", "reveal_duration"], 0.05, 4.0, 0.05],
	["Slope at", ["chapter", "slope_time"], 0.0, 63.936, 0.01],
	["Chart fade", ["chapter", "whittaker_fade_duration"], 0.05, 4.0, 0.05],
]

var _rig: Node3D
var _timeline := {}
var _chapter_index := -1
var _syncing := false

var _root: PanelContainer
var _time_slider: HSlider
var _time_label: Label
var _play_button: Button
var _save_button: Button
var _chapter_label: Label
var _content_box: VBoxContainer
var _camera_box: VBoxContainer
var _chapter_rows: Array = []


func setup(p_rig: Node3D) -> void:
	_rig = p_rig
	_timeline = p_rig.timeline
	layer = 10
	_build_ui()
	_rebuild_chapter_rows(0)


func sync_time(p_time: float) -> void:
	_syncing = true
	_time_slider.value = p_time
	_syncing = false

	var index: int = _rig.get_chapter_index(p_time)
	_time_label.text = "%6.3f s   f%d" % [p_time, roundi(p_time * 60.0)]
	if index != _chapter_index:
		_rebuild_chapter_rows(index)


func _build_ui() -> void:
	_root = PanelContainer.new()
	_root.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	_root.offset_right = PANEL_WIDTH
	_root.mouse_filter = Control.MOUSE_FILTER_PASS

	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.03, 0.05, 0.07, 0.93)
	style.border_color = Color(0.3, 0.82, 1.0, 0.5)
	style.border_width_right = 2
	style.content_margin_left = 12
	style.content_margin_right = 12
	style.content_margin_top = 10
	style.content_margin_bottom = 10
	_root.add_theme_stylebox_override("panel", style)
	add_child(_root)

	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_root.add_child(scroll)

	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_theme_constant_override("separation", 6)
	scroll.add_child(column)

	column.add_child(_heading("TRAILER CONTROLS"))
	column.add_child(_hint("space play/pause   ←→ step   [ ] cue   H hide   S save"))

	# --- transport
	_time_label = Label.new()
	_time_label.add_theme_font_size_override("font_size", 15)
	column.add_child(_time_label)

	var window: Vector2 = _rig.get_shot_window()
	_time_slider = HSlider.new()
	_time_slider.min_value = window.x
	_time_slider.max_value = window.y
	_time_slider.step = 1.0 / 60.0
	_time_slider.value_changed.connect(_on_time_slider_changed)
	column.add_child(_time_slider)

	var transport := HBoxContainer.new()
	_play_button = _button("Pause", _on_play_pressed)
	transport.add_child(_play_button)
	transport.add_child(_button("|< cue", func(): _jump_cue(-1)))
	transport.add_child(_button("cue >|", func(): _jump_cue(1)))
	_save_button = _button("Save", _on_save_pressed)
	transport.add_child(_save_button)
	column.add_child(transport)

	# --- chapter jumps
	var chapters := HBoxContainer.new()
	chapters.add_theme_constant_override("separation", 3)
	var chapter_count: int = _timeline["chapters"].size()
	for i in chapter_count:
		var index: int = i
		var chapter: Dictionary = _timeline["chapters"][i]
		var button := _button(str(chapter["name"]).substr(0, 5), func(): _rig.set_time(float(
				(_timeline["chapters"][index] as Dictionary)["start"])))
		button.add_theme_font_size_override("font_size", 11)
		chapters.add_child(button)
	column.add_child(chapters)

	# --- per-chapter sections
	_chapter_label = _heading("CHAPTER")
	column.add_child(_chapter_label)
	_camera_box = VBoxContainer.new()
	column.add_child(_camera_box)
	_content_box = VBoxContainer.new()
	column.add_child(_content_box)

	# --- global sections
	column.add_child(_heading("LOOK"))
	for row in LOOK_ROWS:
		column.add_child(_slider_row(row))
	column.add_child(_heading("EFFECTS"))
	for row in EFFECT_ROWS:
		column.add_child(_slider_row(row))


# Rebuilds the camera/content rows for whichever chapter the playhead is in. Rows whose key the
# chapter does not define are skipped, so each beat shows only the knobs it actually has.
func _rebuild_chapter_rows(p_index: int) -> void:
	_chapter_index = p_index
	var chapter: Dictionary = _timeline["chapters"][p_index]
	_chapter_label.text = "CHAPTER  %d/%d  %s" % [p_index + 1, _timeline["chapters"].size(),
			str(chapter["name"]).to_upper()]

	for child in _camera_box.get_children():
		child.queue_free()
	for child in _content_box.get_children():
		child.queue_free()
	_chapter_rows.clear()

	for row in CAMERA_ROWS:
		_camera_box.add_child(_slider_row(row))
	for row in CONTENT_ROWS:
		var key: String = row[1][row[1].size() - 1]
		if chapter.has(key):
			_content_box.add_child(_slider_row(row))


func _slider_row(p_row: Array) -> HBoxContainer:
	var box := HBoxContainer.new()

	var label := Label.new()
	label.text = p_row[0]
	label.custom_minimum_size.x = LABEL_WIDTH
	label.add_theme_font_size_override("font_size", 13)
	box.add_child(label)

	var slider := HSlider.new()
	slider.min_value = p_row[2]
	slider.max_value = p_row[3]
	slider.step = p_row[4]
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.value = _read_path(p_row[1])
	box.add_child(slider)

	var value := Label.new()
	value.custom_minimum_size.x = VALUE_WIDTH
	value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	value.add_theme_font_size_override("font_size", 13)
	value.text = _format(slider.value)
	box.add_child(value)

	var path: Array = p_row[1]
	slider.value_changed.connect(func(v: float):
		_write_path(path, v)
		value.text = _format(v)
		_rig.refresh())
	_chapter_rows.append(slider)
	return box


# Paths starting with "chapter" resolve against the chapter currently on screen; everything else is
# an absolute path into the timeline dictionary.
func _resolve(p_path: Array) -> Dictionary:
	var container: Variant = _timeline
	var start := 0
	if p_path[0] == "chapter":
		container = _timeline["chapters"][maxi(_chapter_index, 0)]
		start = 1
	for i in range(start, p_path.size() - 1):
		container = container[p_path[i]]
	return {"container": container, "key": p_path[p_path.size() - 1]}


func _read_path(p_path: Array) -> float:
	var target := _resolve(p_path)
	var container: Variant = target["container"]
	if not container.has(target["key"]):
		return 0.0
	return float(container[target["key"]])


func _write_path(p_path: Array, p_value: float) -> void:
	var target := _resolve(p_path)
	var container: Variant = target["container"]
	container[target["key"]] = p_value


func _on_time_slider_changed(p_value: float) -> void:
	if _syncing:
		return
	_rig.set_playing(false)
	_play_button.text = "Play"
	_rig.set_time(p_value)


func _on_play_pressed() -> void:
	_rig.set_playing(not _rig.is_playing())
	_play_button.text = "Pause" if _rig.is_playing() else "Play"


func _on_save_pressed() -> void:
	_save_button.text = "Saved" if _rig.save_timeline() else "Failed"
	await get_tree().create_timer(1.2).timeout
	if is_instance_valid(_save_button):
		_save_button.text = "Save"


func _jump_cue(p_direction: int) -> void:
	var cues: PackedFloat32Array = _rig.get_cue_times()
	var now: float = _rig.get_time()
	var target := now
	if p_direction > 0:
		for cue in cues:
			if cue > now + CUE_EPSILON:
				target = cue
				break
	else:
		for i in range(cues.size() - 1, -1, -1):
			if cues[i] < now - CUE_EPSILON:
				target = cues[i]
				break
	_rig.set_playing(false)
	_play_button.text = "Play"
	_rig.set_time(target)


func _unhandled_input(p_event: InputEvent) -> void:
	if not (p_event is InputEventKey) or not p_event.pressed or p_event.echo:
		return

	var step := 1.0 if p_event.shift_pressed else 1.0 / 60.0
	match p_event.keycode:
		KEY_SPACE:
			_on_play_pressed()
		KEY_LEFT:
			_rig.set_playing(false)
			_play_button.text = "Play"
			_rig.set_time(_rig.get_time() - step)
		KEY_RIGHT:
			_rig.set_playing(false)
			_play_button.text = "Play"
			_rig.set_time(_rig.get_time() + step)
		KEY_BRACKETLEFT:
			_jump_cue(-1)
		KEY_BRACKETRIGHT:
			_jump_cue(1)
		KEY_H:
			_root.visible = not _root.visible
		KEY_S:
			_on_save_pressed()
		_:
			return
	get_viewport().set_input_as_handled()


func _heading(p_text: String) -> Label:
	var label := Label.new()
	label.text = p_text
	label.add_theme_font_size_override("font_size", 14)
	label.add_theme_color_override("font_color", Color(0.45, 0.88, 1.0))
	return label


func _hint(p_text: String) -> Label:
	var label := Label.new()
	label.text = p_text
	label.add_theme_font_size_override("font_size", 11)
	label.add_theme_color_override("font_color", Color(0.6, 0.7, 0.78))
	return label


func _button(p_text: String, p_callback: Callable) -> Button:
	var button := Button.new()
	button.text = p_text
	button.pressed.connect(p_callback)
	return button


func _format(p_value: float) -> String:
	if absf(p_value) >= 100.0:
		return "%.0f" % p_value
	if absf(p_value) >= 10.0:
		return "%.1f" % p_value
	return "%.3f" % p_value

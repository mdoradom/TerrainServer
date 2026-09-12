extends CanvasLayer

# Live tuning panel for the trailer's build-up shot, enabled with `--controls` (see
# TODO_TRAILER.md T8/T9). It edits trailer_timeline.json in memory and writes it back on Save, so
# what you dial in by eye is exactly what the next --write-movie records.
#
# The panel is never up during a recording -- trailer_rig.gd refuses to enable it when
# --write-movie is present, because it draws into the same viewport the movie captures.
#
# Two surfaces: a bottom bar (transport + the timeline ruler, which draws the music's waveform,
# the detected cues and the chapter bands) and a left column of parameter rows. Every row is
# declared as a path into the timeline dictionary rather than wired by hand: GDScript dictionaries
# are references, so writing through the path mutates the live timeline and the rig re-applies the
# current frame immediately.
#
# Editing is undoable and reversible: the panel keeps the file's contents as loaded, marks every row
# that differs from it, and can put any row -- or the whole file -- back.

const RULER := preload("res://trailer/trailer_timeline_ruler.gd")

const PANEL_WIDTH := 512.0
const BOTTOM_HEIGHT := 192.0
const LABEL_WIDTH := 132.0
const SPIN_WIDTH := 82.0
const CUE_EPSILON := 0.02

# Drag a slider and you get one `value_changed` per frame; coalescing them into a single undo entry
# is what makes Ctrl+Z step back by gesture instead of by frame.
const UNDO_COALESCE_MS := 600
const UNDO_LIMIT := 256

# Loop modes for the transport.
const LOOP_SHOT := 0
const LOOP_CHAPTER := 1
const LOOP_REGION := 2

# Row declaration: [label, path, min, max, step] plus an optional TIME marker for rows whose value
# is a moment on the track. Those get "set to playhead" / "snap to nearest cue" buttons, which is
# the whole job when the thing you are authoring has to land on a drum hit.
#
# Every TIME row steps in milliseconds. Both widgets quantise to their step, so a coarser one would
# display 43.537 as 43.54 and write that back the moment the row was touched -- silently sliding an
# authored moment off the cue it was placed on.
const TIME := "time"

# "chapter" paths are resolved against whichever chapter is on screen, so the same rows follow the
# playhead from beat to beat.
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
	["Sweep glow", ["look", "sweep_glow_width"], 1.0, 120.0, 0.5],
	["Vignette", ["look", "vignette"], 0.0, 1.0, 0.01],
	["Wire red", ["look", "wire_color", 0], 0.0, 1.0, 0.01],
	["Wire green", ["look", "wire_color", 1], 0.0, 1.0, 0.01],
	["Wire blue", ["look", "wire_color", 2], 0.0, 1.0, 0.01],
]

const EFFECT_ROWS := [
	["Pulse decay", ["effects", "pulse_decay"], 0.01, 1.0, 0.005],
	["Pulse gain", ["effects", "pulse_gain"], 0.0, 8.0, 0.05],
	["Onset amplitude", ["effects", "pulse_onset_amplitude"], 0.0, 1.0, 0.01],
	["Pulse by strength", ["effects", "pulse_strength_weight"], 0.0, 1.0, 0.01],
	["Ripple speed", ["effects", "ripple_speed"], 0.0, 4000.0, 10.0],
	["Ripple decay", ["effects", "ripple_decay"], 0.05, 2.0, 0.01],
	["Ripple lift", ["effects", "ripple_lift"], 0.0, 120.0, 0.5],
	["Ripple width", ["effects", "ripple_width"], 5.0, 400.0, 1.0],
	["Cut settle", ["effects", "cut_settle"], 0.0, 0.2, 0.001],
	["Settle time", ["effects", "cut_settle_duration"], 0.1, 8.0, 0.1],
	["FOV punch", ["effects", "fov_punch"], 0.0, 0.2, 0.001],
	["Light angle", ["effects", "light_base_angle"], 0.0, 360.0, 1.0],
	["Light rate", ["effects", "light_rate"], 0.0, 1.0, 0.01],
	["Light start", ["effects", "light_start"], 0.0, 145.0, 0.001, TIME],
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
	["Yaw drift/s", ["chapter", "camera", "yaw_rate"], -12.0, 12.0, 0.05],
	["Pitch drift/s", ["chapter", "camera", "pitch_rate"], -6.0, 6.0, 0.05],
	["Dolly/s", ["chapter", "camera", "distance_rate"], -150.0, 150.0, 0.5],
	["Rise/s", ["chapter", "camera", "height_rate"], -60.0, 60.0, 0.5],
	["Blend in", ["chapter", "camera", "blend_in"], 0.0, 6.0, 0.05],
	["Cut at", ["chapter", "start"], 0.0, 145.0, 0.001, TIME],
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
	["Skirt at", ["chapter", "skirt_start"], 0.0, 145.0, 0.001, TIME],
	["Skirt time", ["chapter", "skirt_duration"], 0.1, 6.0, 0.05],
	["Ridge at", ["chapter", "ridge_time"], 0.0, 145.0, 0.001, TIME],
	["Warp at", ["chapter", "warp_time"], 0.0, 145.0, 0.001, TIME],
	["Continent at", ["chapter", "continent_time"], 0.0, 145.0, 0.001, TIME],
	["Redistrib at", ["chapter", "redistribution_time"], 0.0, 145.0, 0.001, TIME],
	["Lit at", ["chapter", "lit_time"], 0.0, 145.0, 0.001, TIME],
	["Lit time", ["chapter", "lit_duration"], 0.05, 6.0, 0.05],
	["Moisture at", ["chapter", "moisture_time"], 0.0, 145.0, 0.001, TIME],
	["Breath at", ["chapter", "breath_start"], 0.0, 145.0, 0.001, TIME],
	["Breath time", ["chapter", "breath_duration"], 0.1, 12.0, 0.1],
	["Reveal time", ["chapter", "reveal_duration"], 0.05, 4.0, 0.05],
	["Slope at", ["chapter", "slope_time"], 0.0, 145.0, 0.001, TIME],
	["Chart fade", ["chapter", "whittaker_fade_duration"], 0.05, 4.0, 0.05],
]

var _rig: Node3D
var _timeline := {}
# The file as it currently sits on disk, so every row can say whether it has been touched and put
# itself back. Deep-copied: the live timeline is mutated in place through row paths.
var _saved := {}
var _chapter_index := -1
var _syncing := false

var _undo: Array[Dictionary] = []
var _redo: Array[Dictionary] = []

var _loop_mode := LOOP_SHOT

var _left: PanelContainer
var _bottom: PanelContainer
var _ruler: Control
var _time_label: Label
var _status_label: Label
var _play_button: Button
var _save_button: Button
var _loop_button: Button
var _music_button: Button
var _chapter_label: Label
var _filter: LineEdit
var _content_box: VBoxContainer
var _camera_box: VBoxContainer
var _rows: Array[Dictionary] = []


func setup(p_rig: Node3D) -> void:
	_rig = p_rig
	_timeline = p_rig.timeline
	_saved = _timeline.duplicate(true)
	layer = 10
	_build_ui()
	_rebuild_chapter_rows(0)


func sync_time(p_time: float) -> void:
	_ruler.set_time(p_time)

	var index: int = _rig.get_chapter_index(p_time)
	if index != _chapter_index:
		_rebuild_chapter_rows(index)
		if _loop_mode == LOOP_CHAPTER:
			_apply_loop_mode()

	var chapter: Dictionary = _timeline["chapters"][maxi(index, 0)]
	_time_label.text = "%7.3f s    f%-5d   %d/%d  %s" % [p_time, roundi(p_time * 60.0),
			index + 1, _timeline["chapters"].size(), str(chapter["name"]).to_upper()]


# --- UI construction ------------------------------------------------------------------------

func _build_ui() -> void:
	_build_bottom_bar()
	_build_left_panel()
	_refresh_status()


func _build_bottom_bar() -> void:
	_bottom = PanelContainer.new()
	_bottom.set_anchors_preset(Control.PRESET_BOTTOM_WIDE)
	_bottom.offset_top = -BOTTOM_HEIGHT
	_bottom.add_theme_stylebox_override("panel", _panel_style(0))
	add_child(_bottom)

	var column := VBoxContainer.new()
	column.add_theme_constant_override("separation", 5)
	_bottom.add_child(column)

	var header := HBoxContainer.new()
	_time_label = Label.new()
	_time_label.add_theme_font_size_override("font_size", 18)
	_time_label.add_theme_color_override("font_color", Color(1.0, 0.86, 0.35))
	_time_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(_time_label)

	_status_label = Label.new()
	_status_label.add_theme_font_size_override("font_size", 13)
	_status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	header.add_child(_status_label)
	column.add_child(header)

	# Wrapping, so the transport degrades gracefully instead of overflowing at narrow widths.
	var transport := HFlowContainer.new()
	transport.add_theme_constant_override("h_separation", 4)
	_play_button = _button("Pause", _on_play_pressed, "Play / pause  (Space)")
	transport.add_child(_play_button)
	transport.add_child(_button("|< cue", func(): _jump_cue(-1), "Previous audio cue  ([)"))
	transport.add_child(_button("cue >|", func(): _jump_cue(1), "Next audio cue  (])"))
	transport.add_child(_button("|< ch", func(): _jump_chapter(-1), "Previous chapter  (,)"))
	transport.add_child(_button("ch >|", func(): _jump_chapter(1), "Next chapter  (.)"))
	transport.add_child(_button("-1f", func(): _step(-1.0 / 60.0), "Back one frame  (Left)"))
	transport.add_child(_button("+1f", func(): _step(1.0 / 60.0), "Forward one frame  (Right)"))

	var speed := OptionButton.new()
	speed.tooltip_text = "Preview playback speed"
	for label in ["0.25x", "0.5x", "1x"]:
		speed.add_item(label)
	speed.selected = 2
	speed.item_selected.connect(func(i: int): _rig.set_speed([0.25, 0.5, 1.0][i]))
	transport.add_child(speed)

	_loop_button = _button("Loop: shot", _cycle_loop_mode,
			"Cycle shot / chapter looping  (L).  Drag on the ruler with the right button for a free region.")
	transport.add_child(_loop_button)

	# Judging whether a cut lands on a hit is an ear job, so the track plays under the preview and
	# drives the clock while it does. Absent only when there is no preview audio to play.
	if _rig.has_music():
		_music_button = _button("Music: on", _toggle_music, "Play the track under the preview  (M)")
		transport.add_child(_music_button)

		var volume := HSlider.new()
		volume.min_value = -40.0
		volume.max_value = 6.0
		volume.step = 1.0
		volume.value = 0.0
		volume.custom_minimum_size.x = 92.0
		volume.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		volume.tooltip_text = "Music volume (dB)"
		volume.value_changed.connect(func(v: float): _rig.set_music_volume(v))
		transport.add_child(volume)
	else:
		var missing := _hint("no preview audio")
		missing.tooltip_text = "Run demo/trailer/tools/make_preview_audio.sh to generate it"
		transport.add_child(missing)

	transport.add_child(_button("Undo", _undo_last, "Undo the last edit  (Ctrl+Z)"))
	transport.add_child(_button("Redo", _redo_last, "Redo  (Ctrl+Shift+Z)"))
	_save_button = _button("Save", _on_save_pressed, "Write trailer_timeline.json  (Ctrl+S)")
	transport.add_child(_save_button)
	transport.add_child(_button("Revert all", _on_revert_all_pressed,
			"Throw away every unsaved edit and re-read the file"))
	column.add_child(transport)

	_ruler = RULER.new()
	_ruler.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_ruler.size_flags_vertical = Control.SIZE_EXPAND_FILL
	column.add_child(_ruler)
	_ruler.setup(_rig)
	_ruler.seek_requested.connect(_on_seek_requested)
	_ruler.loop_changed.connect(_on_loop_changed)


func _build_left_panel() -> void:
	_left = PanelContainer.new()
	_left.set_anchors_preset(Control.PRESET_LEFT_WIDE)
	_left.offset_right = PANEL_WIDTH
	_left.offset_bottom = -BOTTOM_HEIGHT
	_left.mouse_filter = Control.MOUSE_FILTER_PASS
	_left.add_theme_stylebox_override("panel", _panel_style(2))
	add_child(_left)

	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_left.add_child(scroll)

	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	column.add_theme_constant_override("separation", 5)
	scroll.add_child(column)

	column.add_child(_heading("TRAILER CONTROLS"))
	column.add_child(_hint("space play   <- -> step   [ ] cue   , . chapter   L loop   M music"))
	column.add_child(_hint("ctrl+S save   ctrl+Z undo   1-6 chapter   H hide"))

	_filter = LineEdit.new()
	_filter.placeholder_text = "filter parameters..."
	_filter.clear_button_enabled = true
	_filter.text_changed.connect(_on_filter_changed)
	column.add_child(_filter)

	var chapters := HFlowContainer.new()
	chapters.add_theme_constant_override("h_separation", 3)
	for i in _timeline["chapters"].size():
		var index: int = i
		var chapter: Dictionary = _timeline["chapters"][i]
		var button := _button(str(chapter["name"]).substr(0, 6),
				func(): _rig.set_time(float((_timeline["chapters"][index] as Dictionary)["start"])),
				"Jump to this chapter  (%d)" % (index + 1))
		button.add_theme_font_size_override("font_size", 11)
		chapters.add_child(button)
	column.add_child(chapters)

	_chapter_label = _heading("CHAPTER")
	column.add_child(_chapter_label)
	_camera_box = _section(column, "CAMERA")
	_content_box = _section(column, "CONTENT")

	_section_rows(_section(column, "LOOK"), LOOK_ROWS)
	_section_rows(_section(column, "EFFECTS"), EFFECT_ROWS)


# A collapsible block. The heading doubles as the toggle, so a long parameter list can be folded
# down to the two or three sections actually being worked on.
func _section(p_column: VBoxContainer, p_title: String) -> VBoxContainer:
	var box := VBoxContainer.new()
	if not p_title.is_empty():
		var toggle := Button.new()
		toggle.text = "v  " + p_title
		toggle.alignment = HORIZONTAL_ALIGNMENT_LEFT
		toggle.flat = true
		toggle.add_theme_font_size_override("font_size", 14)
		toggle.add_theme_color_override("font_color", Color(0.45, 0.88, 1.0))
		toggle.pressed.connect(func():
			box.visible = not box.visible
			toggle.text = ("v  " if box.visible else ">  ") + p_title)
		p_column.add_child(toggle)
	p_column.add_child(box)
	return box


func _section_rows(p_box: VBoxContainer, p_rows: Array) -> void:
	for row in p_rows:
		p_box.add_child(_slider_row(row))


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
	# The freed rows are still in the tree this frame; drop them from the registry now so filtering
	# and value syncing never touch a row that is on its way out.
	_rows = _rows.filter(func(r: Dictionary) -> bool: return r["path"][0] != "chapter")

	for row in CAMERA_ROWS:
		_camera_box.add_child(_slider_row(row))
	for row in CONTENT_ROWS:
		var key: String = row[1][row[1].size() - 1]
		if chapter.has(key):
			_content_box.add_child(_slider_row(row))

	_apply_filter(_filter.text if _filter != null else "")


func _slider_row(p_row: Array) -> HBoxContainer:
	var path: Array = p_row[1]
	var is_time: bool = p_row.size() > 5 and p_row[5] == TIME

	var box := HBoxContainer.new()
	box.add_theme_constant_override("separation", 3)

	var dot := Label.new()
	dot.text = "*"
	dot.custom_minimum_size.x = 9.0
	dot.add_theme_font_size_override("font_size", 13)
	dot.add_theme_color_override("font_color", Color(1.0, 0.72, 0.25))
	box.add_child(dot)

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
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	box.add_child(slider)

	# The slider is for feeling out a value, the spin box for landing on an exact one -- times in
	# particular have to match a cue to the millisecond, which no drag can do.
	var spin := SpinBox.new()
	spin.min_value = p_row[2]
	spin.max_value = p_row[3]
	spin.step = p_row[4]
	spin.custom_minimum_size.x = SPIN_WIDTH
	spin.select_all_on_focus = true
	box.add_child(spin)

	var row := {
		"path": path, "label": str(p_row[0]), "slider": slider, "spin": spin, "dot": dot,
		"box": box, "time": is_time, "applying": false,
	}

	var revert := _button("rev", func(): _revert_row(row), "Revert to the saved value")
	revert.add_theme_font_size_override("font_size", 10)
	box.add_child(revert)

	if is_time:
		var at_playhead := _button("@t", func(): _set_row_value(row, _rig.get_time(), true),
				"Set to the playhead")
		at_playhead.add_theme_font_size_override("font_size", 10)
		box.add_child(at_playhead)
		var at_cue := _button("@c", func(): _set_row_value(row, _nearest_cue(_read_path(path)), true),
				"Snap to the nearest audio cue")
		at_cue.add_theme_font_size_override("font_size", 10)
		box.add_child(at_cue)

	var value := _read_path(path)
	slider.value = value
	spin.value = value

	slider.value_changed.connect(func(v: float): _on_row_edited(row, v, slider))
	spin.value_changed.connect(func(v: float): _on_row_edited(row, v, spin))

	_rows.append(row)
	_refresh_dot(row)
	return box


# --- Editing --------------------------------------------------------------------------------

# Mirrors the edit onto whichever of the row's two widgets did not originate it, then commits.
func _on_row_edited(p_row: Dictionary, p_value: float, p_source: Control) -> void:
	if _syncing or p_row["applying"]:
		return
	p_row["applying"] = true
	if p_source == p_row["slider"]:
		(p_row["spin"] as SpinBox).value = p_value
	else:
		(p_row["slider"] as HSlider).value = p_value
	p_row["applying"] = false
	_commit(p_row, p_value)


func _set_row_value(p_row: Dictionary, p_value: float, p_commit: bool) -> void:
	p_row["applying"] = true
	(p_row["slider"] as HSlider).value = p_value
	(p_row["spin"] as SpinBox).value = p_value
	p_row["applying"] = false
	if p_commit:
		_commit(p_row, p_value)


func _commit(p_row: Dictionary, p_value: float) -> void:
	var path: Array = p_row["path"]
	var target := _resolve(path)
	var container: Variant = target["container"]
	var key: Variant = target["key"]
	var old := _read_path(path)
	if is_equal_approx(old, p_value):
		return

	_push_undo(container, key, old, p_value)
	container[key] = p_value
	_redo.clear()

	_refresh_dot(p_row)
	_refresh_status()
	_rig.refresh()
	# Chapter starts and cue-locked moments move the bands and boundaries the ruler paints.
	if p_row["time"]:
		_ruler.refresh_lanes()


func _push_undo(p_container: Variant, p_key: Variant, p_old: float, p_new: float) -> void:
	var stamp := Time.get_ticks_msec()
	if not _undo.is_empty():
		var last: Dictionary = _undo[_undo.size() - 1]
		# is_same(), not ==: Dictionary equality compares contents, and two chapters can hold
		# identical values while being different chapters.
		if is_same(last["container"], p_container) and last["key"] == p_key \
				and stamp - int(last["stamp"]) < UNDO_COALESCE_MS:
			last["new"] = p_new
			last["stamp"] = stamp
			return

	_undo.append({"container": p_container, "key": p_key, "old": p_old, "new": p_new, "stamp": stamp})
	if _undo.size() > UNDO_LIMIT:
		_undo.remove_at(0)


func _undo_last() -> void:
	if _undo.is_empty():
		_flash_status("nothing to undo")
		return
	var entry: Dictionary = _undo.pop_back()
	entry["container"][entry["key"]] = entry["old"]
	_redo.append(entry)
	_after_bulk_change()


func _redo_last() -> void:
	if _redo.is_empty():
		_flash_status("nothing to redo")
		return
	var entry: Dictionary = _redo.pop_back()
	entry["container"][entry["key"]] = entry["new"]
	_undo.append(entry)
	_after_bulk_change()


func _revert_row(p_row: Dictionary) -> void:
	var saved := _read_from(_saved, p_row["path"])
	_set_row_value(p_row, saved, true)


# Re-reads every visible row off the timeline. Used after any change that did not come from a row
# widget (undo, redo, revert-all), so the panel never shows a value the timeline no longer holds.
func _after_bulk_change() -> void:
	_syncing = true
	for row in _rows:
		var value := _read_path(row["path"])
		(row["slider"] as HSlider).value = value
		(row["spin"] as SpinBox).value = value
		_refresh_dot(row)
	_syncing = false
	_refresh_status()
	_rig.refresh()
	_ruler.refresh_lanes()


# --- Paths ----------------------------------------------------------------------------------

# Paths starting with "chapter" resolve against the chapter currently on screen; everything else is
# an absolute path into the timeline dictionary.
func _resolve(p_path: Array) -> Dictionary:
	return _resolve_in(_timeline, p_path)


func _resolve_in(p_root: Dictionary, p_path: Array) -> Dictionary:
	var container: Variant = p_root
	var start := 0
	if p_path[0] == "chapter":
		container = p_root["chapters"][maxi(_chapter_index, 0)]
		start = 1
	for i in range(start, p_path.size() - 1):
		container = container[p_path[i]]
	return {"container": container, "key": p_path[p_path.size() - 1]}


func _read_path(p_path: Array) -> float:
	return _read_from(_timeline, p_path)


# Paths bottom out in either a dictionary key or an array index -- look.wire_color is three floats
# in a list -- so the container's type decides how "is this key present" is even asked.
func _read_from(p_root: Dictionary, p_path: Array) -> float:
	var target := _resolve_in(p_root, p_path)
	var container: Variant = target["container"]
	var key: Variant = target["key"]
	if container is Array:
		var array: Array = container
		var index := int(key)
		return float(array[index]) if index >= 0 and index < array.size() else 0.0
	var dictionary: Dictionary = container
	if not dictionary.has(key):
		return 0.0
	return float(dictionary[key])


# --- Transport ------------------------------------------------------------------------------

func _on_seek_requested(p_time: float) -> void:
	_pause()
	_rig.set_time(p_time)


func _on_loop_changed(p_from: float, p_to: float) -> void:
	if p_to <= p_from:
		_loop_mode = LOOP_SHOT
	else:
		_loop_mode = LOOP_REGION
	_rig.set_loop_region(p_from, p_to)
	_refresh_loop_button()


func _cycle_loop_mode() -> void:
	_loop_mode = LOOP_SHOT if _loop_mode != LOOP_SHOT else LOOP_CHAPTER
	_apply_loop_mode()


func _apply_loop_mode() -> void:
	match _loop_mode:
		LOOP_CHAPTER:
			var chapters: Array = _timeline["chapters"]
			var index := maxi(_chapter_index, 0)
			var window: Vector2 = _rig.get_shot_window()
			var from := float(chapters[index]["start"])
			var to := window.y if index == chapters.size() - 1 else float(chapters[index + 1]["start"])
			_rig.set_loop_region(from, to)
			_ruler.set_loop(from, to)
		LOOP_SHOT:
			_rig.set_loop_region(-1.0, -1.0)
			_ruler.set_loop(-1.0, -1.0)
	_refresh_loop_button()


func _refresh_loop_button() -> void:
	_loop_button.text = ["Loop: shot", "Loop: chapter", "Loop: region"][_loop_mode]


func _toggle_music() -> void:
	var enabled: bool = not _rig.is_music_enabled()
	_rig.set_music_enabled(enabled)
	_music_button.text = "Music: on" if enabled else "Music: off"


func _on_play_pressed() -> void:
	_rig.set_playing(not _rig.is_playing())
	_play_button.text = "Pause" if _rig.is_playing() else "Play"


func _pause() -> void:
	_rig.set_playing(false)
	_play_button.text = "Play"


func _step(p_delta: float) -> void:
	_pause()
	_rig.set_time(_rig.get_time() + p_delta)


func _on_save_pressed() -> void:
	if _rig.save_timeline():
		_saved = _timeline.duplicate(true)
		for row in _rows:
			_refresh_dot(row)
		_flash_status("saved")
	else:
		_flash_status("SAVE FAILED")
	_refresh_status()


func _on_revert_all_pressed() -> void:
	if not _rig.reload_timeline():
		_flash_status("RELOAD FAILED")
		return
	# reload_timeline() parses into a fresh dictionary, so every reference the panel holds is stale.
	_timeline = _rig.timeline
	_saved = _timeline.duplicate(true)
	_undo.clear()
	_redo.clear()
	_rebuild_chapter_rows(maxi(_chapter_index, 0))
	_after_bulk_change()
	_flash_status("reverted to file")


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
	_pause()
	_rig.set_time(target)


func _jump_chapter(p_direction: int) -> void:
	var chapters: Array = _timeline["chapters"]
	var index := clampi(_chapter_index + p_direction, 0, chapters.size() - 1)
	_pause()
	_rig.set_time(float(chapters[index]["start"]))


func _nearest_cue(p_time: float) -> float:
	var best := p_time
	var best_distance := INF
	for cue in _rig.get_cue_times():
		var distance: float = absf(cue - p_time)
		if distance < best_distance:
			best_distance = distance
			best = cue
	return best


# --- Status ---------------------------------------------------------------------------------

func _refresh_dot(p_row: Dictionary) -> void:
	var dirty := not is_equal_approx(_read_path(p_row["path"]), _read_from(_saved, p_row["path"]))
	(p_row["dot"] as Label).modulate.a = 1.0 if dirty else 0.0


func _dirty_count() -> int:
	var count := 0
	for row in _rows:
		if (row["dot"] as Label).modulate.a > 0.5:
			count += 1
	return count


func _refresh_status() -> void:
	var dirty := _dirty_count()
	_status_label.text = "%d unsaved" % dirty if dirty > 0 else "saved"
	_status_label.add_theme_color_override("font_color",
			Color(1.0, 0.72, 0.25) if dirty > 0 else Color(0.45, 0.75, 0.55))


func _flash_status(p_text: String) -> void:
	_status_label.text = p_text
	await get_tree().create_timer(1.4).timeout
	if is_instance_valid(_status_label):
		_refresh_status()


# --- Filtering ------------------------------------------------------------------------------

func _on_filter_changed(p_text: String) -> void:
	_apply_filter(p_text)


func _apply_filter(p_text: String) -> void:
	var needle := p_text.strip_edges().to_lower()
	for row in _rows:
		var box: Control = row["box"]
		if is_instance_valid(box):
			box.visible = needle.is_empty() or str(row["label"]).to_lower().contains(needle)


# --- Input ----------------------------------------------------------------------------------

func _unhandled_input(p_event: InputEvent) -> void:
	if not (p_event is InputEventKey) or not p_event.pressed or p_event.echo:
		return

	# Typing in the filter box (or a spin box) must not also drive the transport.
	var focus := get_viewport().gui_get_focus_owner()
	if focus is LineEdit or focus is SpinBox:
		if not p_event.ctrl_pressed:
			return

	if p_event.ctrl_pressed:
		match p_event.keycode:
			KEY_S:
				_on_save_pressed()
			KEY_Z:
				if p_event.shift_pressed:
					_redo_last()
				else:
					_undo_last()
			KEY_Y:
				_redo_last()
			_:
				return
		get_viewport().set_input_as_handled()
		return

	var step := 1.0 if p_event.shift_pressed else 1.0 / 60.0
	match p_event.keycode:
		KEY_SPACE:
			_on_play_pressed()
		KEY_LEFT:
			_step(-step)
		KEY_RIGHT:
			_step(step)
		KEY_BRACKETLEFT:
			_jump_cue(-1)
		KEY_BRACKETRIGHT:
			_jump_cue(1)
		KEY_COMMA:
			_jump_chapter(-1)
		KEY_PERIOD:
			_jump_chapter(1)
		KEY_L:
			_cycle_loop_mode()
		KEY_M:
			if _rig.has_music():
				_toggle_music()
		KEY_H:
			_left.visible = not _left.visible
			_bottom.visible = _left.visible
		KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9:
			var index: int = p_event.keycode - KEY_1
			var chapters: Array = _timeline["chapters"]
			if index >= chapters.size():
				return
			_pause()
			_rig.set_time(float(chapters[index]["start"]))
		_:
			return
	get_viewport().set_input_as_handled()


# --- Widgets --------------------------------------------------------------------------------

func _panel_style(p_border_right: int) -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = Color(0.03, 0.05, 0.07, 0.93)
	style.border_color = Color(0.3, 0.82, 1.0, 0.5)
	style.border_width_right = p_border_right
	style.border_width_top = 2 if p_border_right == 0 else 0
	style.content_margin_left = 12
	style.content_margin_right = 12
	style.content_margin_top = 8
	style.content_margin_bottom = 8
	return style


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


func _button(p_text: String, p_callback: Callable, p_tooltip := "") -> Button:
	var button := Button.new()
	button.text = p_text
	button.tooltip_text = p_tooltip
	button.pressed.connect(p_callback)
	return button

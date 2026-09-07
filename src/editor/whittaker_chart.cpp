#include "whittaker_chart.h"

#include "core/terrain_biome_layer.h"

#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/theme.hpp>

#include <algorithm>

using namespace godot;

namespace ts {

namespace {

constexpr float CHART_SIZE_X = 280.0f;
constexpr float CHART_SIZE_Y = 180.0f;
constexpr float CHART_MARGIN_LEFT = 42.0f;
constexpr float CHART_MARGIN_BOTTOM = 24.0f;
constexpr float CHART_MARGIN_TOP = 12.0f;
constexpr float CHART_MARGIN_RIGHT = 12.0f;
constexpr float PROBE_RADIUS = 4.0f;
constexpr float TICK_LENGTH = 3.0f;
constexpr float TICK_LABEL_GAP = 4.0f;

constexpr int TEMPERATURE_TICK_COUNT = 5; // -10, 0, 10, 20, 30
constexpr int MOISTURE_TICK_COUNT = 5; // 0, .25, .5, .75, 1

} //namespace

void WhittakerChart::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_configuration", "configuration"), &WhittakerChart::set_configuration);
	ClassDB::bind_method(D_METHOD("get_configuration"), &WhittakerChart::get_configuration);
}

WhittakerChart::WhittakerChart() {
	set_custom_minimum_size(Vector2(CHART_SIZE_X, CHART_SIZE_Y));
}

void WhittakerChart::_ready() {
	_pull_theme();
}

void WhittakerChart::_notification(int p_what) {
	if (p_what == NOTIFICATION_THEME_CHANGED) {
		_pull_theme();
		queue_redraw();
	}
}

void WhittakerChart::_pull_theme() {
	const EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (editor_interface == nullptr) {
		return;
	}

	_editor_scale = editor_interface->get_editor_scale();
	set_custom_minimum_size(Vector2(CHART_SIZE_X, CHART_SIZE_Y) * _editor_scale);

	const Ref<Theme> theme = editor_interface->get_editor_theme();
	if (!theme.is_valid()) {
		return;
	}

	_font = theme->get_font("font", "Label");
	_font_size = theme->get_font_size("font_size", "Label");
	_text_color = theme->get_color("font_color", "Label");
	_axis_color = Color(_text_color, 0.35f);
	_background_color = theme->get_color("dark_color_1", "Editor");
	_probe_color = theme->get_color("accent_color", "Editor");
}

Rect2 WhittakerChart::_plot_rect() const {
	const Vector2 size = get_size();
	const float left = CHART_MARGIN_LEFT * _editor_scale;
	const float top = CHART_MARGIN_TOP * _editor_scale;
	const float right = CHART_MARGIN_RIGHT * _editor_scale;
	const float bottom = CHART_MARGIN_BOTTOM * _editor_scale;
	return Rect2(Vector2(left, top), Vector2(std::max(size.x - left - right, 1.0f), std::max(size.y - top - bottom, 1.0f)));
}

Vector2 WhittakerChart::_to_plot(const float p_temperature, const float p_moisture) const {
	const Rect2 plot = _plot_rect();
	const float tx = (p_temperature - MIN_TEMPERATURE) / (MAX_TEMPERATURE - MIN_TEMPERATURE);
	const float my = (p_moisture - MIN_MOISTURE) / (MAX_MOISTURE - MIN_MOISTURE);
	return plot.position + Vector2(tx * plot.size.x, (1.0f - my) * plot.size.y);
}

void WhittakerChart::_draw() {
	draw_rect(Rect2(Vector2(), get_size()), _background_color);

	const Rect2 plot = _plot_rect();
	draw_line(plot.position + Vector2(0.0f, plot.size.y), plot.position, _axis_color);
	draw_line(plot.position + Vector2(0.0f, plot.size.y), plot.position + plot.size, _axis_color);

	for (int i = 0; i < TEMPERATURE_TICK_COUNT; i++) {
		const float t = MIN_TEMPERATURE + (MAX_TEMPERATURE - MIN_TEMPERATURE) * i / (TEMPERATURE_TICK_COUNT - 1);
		const Vector2 p = _to_plot(t, MIN_MOISTURE);
		draw_line(p, p + Vector2(0.0f, TICK_LENGTH * _editor_scale), _axis_color);
		const Vector2 label_offset(-8.0f, TICK_LENGTH + TICK_LABEL_GAP + _font_size * 0.9f);
		draw_string(_font, p + label_offset * _editor_scale, String::num_int64(static_cast<int64_t>(t)), HORIZONTAL_ALIGNMENT_LEFT, -1, _font_size, _text_color);
	}

	// Fixed two-decimal formatting keeps every moisture label the same width, so the column of
	// numbers lines up instead of drifting left/right as "1.0" vs "0.75" would.
	for (int i = 0; i < MOISTURE_TICK_COUNT; i++) {
		const float m = MIN_MOISTURE + (MAX_MOISTURE - MIN_MOISTURE) * i / (MOISTURE_TICK_COUNT - 1);
		const Vector2 p = _to_plot(MIN_TEMPERATURE, m);
		draw_line(p, p - Vector2(TICK_LENGTH * _editor_scale, 0.0f), _axis_color);
		draw_string(_font, p + Vector2(-(CHART_MARGIN_LEFT - TICK_LABEL_GAP), _font_size * 0.3f) * _editor_scale, vformat("%.2f", m), HORIZONTAL_ALIGNMENT_LEFT, -1, _font_size, _text_color);
	}

	if (_configuration.is_valid()) {
		const TypedArray<TerrainBiomeLayer> layers = _configuration->get_biome_layers();
		const int count = layers.size();
		for (int i = 0; i < count; i++) {
			const Ref<TerrainBiomeLayer> layer = layers[i];
			if (!layer.is_valid()) {
				continue;
			}

			const Vector2 top_left = _to_plot(layer->get_min_temperature(), layer->get_max_moisture());
			const Vector2 bottom_right = _to_plot(layer->get_max_temperature(), layer->get_min_moisture());
			const Rect2 rect(top_left, bottom_right - top_left);

			const Color fill = Color::from_hsv(static_cast<float>(i) / count, 0.55f, 0.85f, 0.35f);
			draw_rect(rect, fill, true);
			draw_rect(rect, Color(fill, 1.0f), false, 1.0f);
			draw_string(_font, top_left + Vector2(4.0f, 12.0f) * _editor_scale, layer->get_biome_name(), HORIZONTAL_ALIGNMENT_LEFT, rect.size.x - 4.0f, _font_size, _text_color);
		}
	}

	if (_has_probe) {
		const Vector2 probe = _to_plot(_probe_temperature, _probe_moisture);
		draw_circle(probe, PROBE_RADIUS * _editor_scale, _probe_color);
		draw_circle(probe, PROBE_RADIUS * _editor_scale, Color(0.0f, 0.0f, 0.0f, 0.8f), false, 1.0f);
	}
}

void WhittakerChart::set_configuration(const Ref<TerrainConfiguration> &p_configuration) {
	_configuration = p_configuration;
	queue_redraw();
}

Ref<TerrainConfiguration> WhittakerChart::get_configuration() const {
	return _configuration;
}

void WhittakerChart::set_probe(const float p_temperature, const float p_moisture) {
	_has_probe = true;
	_probe_temperature = p_temperature;
	_probe_moisture = p_moisture;
	queue_redraw();
}

void WhittakerChart::clear_probe() {
	_has_probe = false;
	queue_redraw();
}

} //namespace ts

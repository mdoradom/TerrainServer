#pragma once

#include "core/terrain_configuration.h"

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/rect2.hpp>
#include <godot_cpp/variant/vector2.hpp>

namespace ts {

// Temperature x moisture plot: one rect per TerrainBiomeLayer plus a live probe dot.
class WhittakerChart : public godot::Control {
	GDCLASS(WhittakerChart, godot::Control);

private:
	static constexpr float MIN_TEMPERATURE = -10.0f;
	static constexpr float MAX_TEMPERATURE = 30.0f;
	static constexpr float MIN_MOISTURE = 0.0f;
	static constexpr float MAX_MOISTURE = 1.0f;

	godot::Ref<TerrainConfiguration> _configuration;
	bool _has_probe = false;
	float _probe_temperature = 0.0f;
	float _probe_moisture = 0.0f;

	godot::Ref<godot::Font> _font;
	int _font_size = 12;
	godot::Color _axis_color = godot::Color(1.0f, 1.0f, 1.0f, 0.3f);
	godot::Color _text_color = godot::Color(1.0f, 1.0f, 1.0f, 0.8f);
	godot::Color _background_color = godot::Color(0.1f, 0.1f, 0.12f, 1.0f);
	godot::Color _probe_color = godot::Color(1.0f, 0.85f, 0.2f, 1.0f);
	float _editor_scale = 1.0f;

	godot::Rect2 _plot_rect() const;
	godot::Vector2 _to_plot(float p_temperature, float p_moisture) const;
	void _pull_theme();

protected:
	static void _bind_methods();

public:
	WhittakerChart();

	void _ready() override;
	void _notification(int p_what);
	void _draw() override;

	void set_configuration(const godot::Ref<TerrainConfiguration> &p_configuration);
	godot::Ref<TerrainConfiguration> get_configuration() const;

	void set_probe(float p_temperature, float p_moisture);
	void clear_probe();
};

} //namespace ts

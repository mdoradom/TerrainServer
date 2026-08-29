#pragma once

#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace ts {

inline constexpr int32_t TEXTURE_SLOT_MAX_DIMENSION = 2048;

// Shared by TerrainBiomeLayer and TerrainSlopeLayer texture setters: rejects+warns on
// oversized textures (old value kept), otherwise reconnects "changed"->emit_changed and
// emits p_owner's own "changed" signal. p_owner must be a Resource (exposes emit_changed()).
inline void set_texture_slot(godot::Object *p_owner, godot::Ref<godot::Texture2D> &r_slot,
		const godot::Ref<godot::Texture2D> &p_texture, const char *p_debug_kind, const char *p_class_name) {
	if (p_texture.is_valid() && (p_texture->get_width() > TEXTURE_SLOT_MAX_DIMENSION || p_texture->get_height() > TEXTURE_SLOT_MAX_DIMENSION)) {
		godot::UtilityFunctions::push_warning(p_class_name, ": ", p_debug_kind, " (", p_texture->get_width(), "x", p_texture->get_height(),
				") exceeds the ", TEXTURE_SLOT_MAX_DIMENSION, "x", TEXTURE_SLOT_MAX_DIMENSION, " size cap; ignoring.");
		return;
	}

	if (r_slot != p_texture) {
		if (r_slot.is_valid()) {
			r_slot->disconnect("changed", godot::Callable(p_owner, "emit_changed"));
		}

		r_slot = p_texture;

		if (r_slot.is_valid()) {
			r_slot->connect("changed", godot::Callable(p_owner, "emit_changed"));
		}

		p_owner->call("emit_changed");
	}
}

} //namespace ts

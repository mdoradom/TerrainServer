#pragma once

#include <godot_cpp/classes/resource.hpp>

namespace ts {

class TerrainConfiguration : public godot::Resource {
	GDCLASS(TerrainConfiguration, godot::Resource);

	private:
		double _height_scale;
		int _seed;
		double _noise_scale;

	protected:
		static void _bind_methods();

	public:
		TerrainConfiguration();
		~TerrainConfiguration();

		// Getters and Setters
		double get_height_scale() const;
		void set_height_scale(double p_scale);

		int get_seed() const;
		void set_seed(int p_seed);

		void set_noise_scale(double p_noise_scale);
		double get_noise_scale() const;
};

}
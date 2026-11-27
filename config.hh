#ifndef FCONFIG_CONFIG_HH
#define FCONFIG_CONFIG_HH

#include <string>
#include <vector>

struct object_config {
    std::string shape;
    bool rigid;
    bool material_enabled;
    bool tracked;
    double center_x;
    double center_y;
    double radius;
    double half_width;
    double half_height;
    double rotation;
    double gravity;
    double velocity_x;
    double velocity_y;
    double angular_velocity;
    double rigid_offset;
    double shear_modulus;
    double solid_density;
    bool set_velocity;
    double spin_scale;
    double spin_a1;
    double spin_a2;
    double spin_pivot_radius;
    double spin_max_angle;
    double spin_angular_velocity;

    object_config();
};

struct simulation_config {
    int grid_resolution;
    int grid_aspect_ratio;
    double duration;
    int num_frames;
    double transition_width;
    double pinning_width;
    int num_tracers;
    unsigned int output_flags;

    double fluid_density;
    double solid_density;
    double viscosity;
    double shear_modulus;
    double reynolds_number;
    double flow_speed;
    std::string flow_type;

    double extra_viscosity_multiplier;
    double transition_viscosity_multiplier;
    double timestep_padding;
    double viscosity_timestep_padding;
    bool set_velocity;
    bool solid_periodic_boundary;

    std::string output_name;

    double domain_x_min;
    double domain_x_max;
    double domain_y_min;
    double domain_y_max;
    bool periodic_x;
    bool periodic_y;

    std::vector<object_config> objects;
};

simulation_config default_config();
bool load_config(const char *filename, simulation_config &config,
                 std::string &error);
bool validate_config(const simulation_config &config, std::string &error);

#endif

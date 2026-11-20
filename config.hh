#ifndef FCONFIG_CONFIG_HH
#define FCONFIG_CONFIG_HH

#include <string>

struct simulation_config {
    int grid_resolution;
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

    double extra_viscosity_multiplier;
    double transition_viscosity_multiplier;
    double timestep_padding;
    double viscosity_timestep_padding;
    bool set_velocity;

    std::string output_name;

    double domain_x_min;
    double domain_x_max;
    double domain_y_min;
    double domain_y_max;
    bool periodic_x;
    bool periodic_y;

    double circle_x;
    double circle_y;
    double circle_radius;
    double circle_velocity_x;
    double circle_velocity_y;
    double circle_angular_velocity;
};

simulation_config default_config();
bool load_config(const char *filename, simulation_config &config,
                 std::string &error);
bool validate_config(const simulation_config &config, std::string &error);

#endif

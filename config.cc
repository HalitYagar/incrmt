#include "config.hh"

#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace {

std::string trim(const std::string &value) {
    std::string::size_type first=0,last=value.size();
    while(first<last&&std::isspace(static_cast<unsigned char>(value[first])))
        first++;
    while(last>first&&std::isspace(static_cast<unsigned char>(value[last-1])))
        last--;
    return value.substr(first,last-first);
}

bool parse_int(const std::string &value,int &result) {
    char *end;
    long parsed;
    errno=0;
    parsed=strtol(value.c_str(),&end,10);
    if(errno==ERANGE||end==value.c_str()||*end!='\0')
        return false;
    result=static_cast<int>(parsed);
    return parsed>=-2147483647L-1&&parsed<=2147483647L;
}

bool parse_unsigned(const std::string &value,unsigned int &result) {
    char *end;
    unsigned long parsed;
    errno=0;
    parsed=strtoul(value.c_str(),&end,10);
    if(errno==ERANGE||end==value.c_str()||*end!='\0')
        return false;
    result=static_cast<unsigned int>(parsed);
    return parsed<=4294967295UL;
}

bool parse_double(const std::string &value,double &result) {
    char *end;
    errno=0;
    result=strtod(value.c_str(),&end);
    return errno!=ERANGE&&end!=value.c_str()&&*end=='\0';
}

bool parse_bool(const std::string &value,bool &result) {
    if(value=="true"||value=="1") {
        result=true;
        return true;
    }
    if(value=="false"||value=="0") {
        result=false;
        return true;
    }
    return false;
}

template<class T>
bool set_value(const std::string &key,const std::string &value,T &result,
              bool (*parser)(const std::string &,T &),std::string &error) {
    if(!parser(value,result)) {
        error="invalid value for '" + key + "': " + value;
        return false;
    }
    return true;
}

bool set_config_value(const std::string &key,const std::string &value,
                      simulation_config &config,std::string &error) {
#define SET_INT(name, field) \
    if(key==name) return set_value(key,value,config.field,parse_int,error)
#define SET_UNSIGNED(name, field) \
    if(key==name) return set_value(key,value,config.field,parse_unsigned,error)
#define SET_DOUBLE(name, field) \
    if(key==name) return set_value(key,value,config.field,parse_double,error)
#define SET_BOOL(name, field) \
    if(key==name) return set_value(key,value,config.field,parse_bool,error)

    SET_INT("grid_resolution",grid_resolution);
    SET_INT("num_frames",num_frames);
    SET_INT("num_tracers",num_tracers);
    SET_UNSIGNED("output_flags",output_flags);
    SET_DOUBLE("duration",duration);
    SET_DOUBLE("transition_width",transition_width);
    SET_DOUBLE("pinning_width",pinning_width);
    SET_DOUBLE("fluid_density",fluid_density);
    SET_DOUBLE("solid_density",solid_density);
    SET_DOUBLE("viscosity",viscosity);
    SET_DOUBLE("shear_modulus",shear_modulus);
    SET_DOUBLE("extra_viscosity_multiplier",extra_viscosity_multiplier);
    SET_DOUBLE("transition_viscosity_multiplier",
               transition_viscosity_multiplier);
    SET_DOUBLE("timestep_padding",timestep_padding);
    SET_DOUBLE("viscosity_timestep_padding",viscosity_timestep_padding);
    SET_BOOL("set_velocity",set_velocity);
    SET_DOUBLE("circle_x",circle_x);
    SET_DOUBLE("circle_y",circle_y);
    SET_DOUBLE("circle_radius",circle_radius);
    SET_DOUBLE("circle_velocity_x",circle_velocity_x);
    SET_DOUBLE("circle_velocity_y",circle_velocity_y);
    SET_DOUBLE("circle_angular_velocity",circle_angular_velocity);
    SET_DOUBLE("domain_x_min",domain_x_min);
    SET_DOUBLE("domain_x_max",domain_x_max);
    SET_DOUBLE("domain_y_min",domain_y_min);
    SET_DOUBLE("domain_y_max",domain_y_max);
    SET_BOOL("periodic_x",periodic_x);
    SET_BOOL("periodic_y",periodic_y);

#undef SET_INT
#undef SET_UNSIGNED
#undef SET_DOUBLE
#undef SET_BOOL

    if(key=="output_name") {
        if(value.empty()) {
            error="output_name cannot be empty";
            return false;
        }
        config.output_name=value;
        return true;
    }
    error="unknown configuration key: " + key;
    return false;
}

}

simulation_config default_config() {
    simulation_config config;
    config.grid_resolution=128;
    config.duration=3.;
    config.num_frames=10;
    config.transition_width=2.5;
    config.pinning_width=2.5;
    config.num_tracers=512;
    config.output_flags=8|16|64|128|256|512;
    config.fluid_density=1.;
    config.solid_density=4.;
    config.viscosity=1e-3;
    config.shear_modulus=24.;
    config.extra_viscosity_multiplier=0.8;
    config.transition_viscosity_multiplier=1.;
    config.timestep_padding=0.4;
    config.viscosity_timestep_padding=0.8;
    config.set_velocity=true;
    config.output_name="circ-move";
    config.domain_x_min=-1.;
    config.domain_x_max=1.;
    config.domain_y_min=-1.;
    config.domain_y_max=1.;
    config.periodic_x=false;
    config.periodic_y=false;
    config.circle_x=-0.5;
    config.circle_y=-0.5;
    config.circle_radius=0.25;
    config.circle_velocity_x=2.;
    config.circle_velocity_y=4.;
    config.circle_angular_velocity=24.;
    return config;
}

bool load_config(const char *filename,simulation_config &config,
                 std::string &error) {
    std::ifstream input(filename);
    if(!input) {
        error="could not open configuration file: " + std::string(filename);
        return false;
    }

    std::string line;
    int line_number=0;
    while(std::getline(input,line)) {
        line_number++;
        std::string::size_type comment=line.find('#');
        if(comment!=std::string::npos) line.erase(comment);
        line=trim(line);
        if(line.empty()) continue;

        std::string::size_type separator=line.find('=');
        if(separator==std::string::npos) {
            std::ostringstream message;
            message<<"line "<<line_number<<" is missing '='";
            error=message.str();
            return false;
        }
        std::string key=trim(line.substr(0,separator));
        std::string value=trim(line.substr(separator+1));
        if(key.empty()||value.empty()) {
            std::ostringstream message;
            message<<"line "<<line_number<<" has an empty key or value";
            error=message.str();
            return false;
        }
        if(!set_config_value(key,value,config,error)) {
            std::ostringstream message;
            message<<"line "<<line_number<<": "<<error;
            error=message.str();
            return false;
        }
    }
    return validate_config(config,error);
}

bool validate_config(const simulation_config &config,std::string &error) {
    if(config.grid_resolution<20||config.grid_resolution>65536)
        error="grid_resolution must be between 20 and 65536";
    else if(config.duration<=0)
        error="duration must be positive";
    else if(config.num_frames<=0)
        error="num_frames must be positive";
    else if(config.transition_width<=0||config.pinning_width<=0)
        error="transition_width and pinning_width must be positive";
    else if(config.num_tracers<0)
        error="num_tracers cannot be negative";
    else if(config.fluid_density<=0||config.solid_density<=0)
        error="fluid_density and solid_density must be positive";
    else if(config.viscosity<=0||config.shear_modulus<=0)
        error="viscosity and shear_modulus must be positive";
    else if(config.extra_viscosity_multiplier<=0||
            config.transition_viscosity_multiplier<=0||
            config.timestep_padding<=0||
            config.viscosity_timestep_padding<=0)
        error="viscosity and timestep multipliers must be positive";
    else if(config.output_name.empty())
        error="output_name cannot be empty";
    else if(config.circle_radius<=0)
        error="circle_radius must be positive";
    else if(config.domain_x_min>=config.domain_x_max||
            config.domain_y_min>=config.domain_y_max)
        error="domain minimums must be less than maximums";
    else
        return true;
    return false;
}

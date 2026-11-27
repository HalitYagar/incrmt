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
    if(errno==ERANGE||end==value.c_str()||*end!='\0'||
       parsed<-2147483647L-1||parsed>2147483647L)
        return false;
    result=static_cast<int>(parsed);
    return true;
}

bool parse_unsigned(const std::string &value,unsigned int &result) {
    char *end;
    unsigned long parsed;
    if(!value.empty()&&value[0]=='-') return false;
    errno=0;
    parsed=strtoul(value.c_str(),&end,10);
    if(errno==ERANGE||end==value.c_str()||*end!='\0'||
       parsed>4294967295UL)
        return false;
    result=static_cast<unsigned int>(parsed);
    return true;
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

bool set_simulation_value(const std::string &key,const std::string &value,
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
    SET_INT("grid_aspect_ratio",grid_aspect_ratio);
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
    SET_DOUBLE("reynolds_number",reynolds_number);
    SET_DOUBLE("flow_speed",flow_speed);
    SET_DOUBLE("extra_viscosity_multiplier",extra_viscosity_multiplier);
    SET_DOUBLE("transition_viscosity_multiplier",
               transition_viscosity_multiplier);
    SET_DOUBLE("timestep_padding",timestep_padding);
    SET_DOUBLE("viscosity_timestep_padding",viscosity_timestep_padding);
    SET_BOOL("set_velocity",set_velocity);
    SET_BOOL("solid_periodic_boundary",solid_periodic_boundary);
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
        config.output_name=value;
        return true;
    }
    if(key=="flow_type") {
        if(value!="horizontal"&&value!="zero") {
            error="flow_type must be 'horizontal' or 'zero'";
            return false;
        }
        config.flow_type=value;
        return true;
    }
    error="unknown simulation key: " + key;
    return false;
}

bool set_object_value(const std::string &key,const std::string &value,
                      object_config &object,std::string &error) {
#define SET_DOUBLE(name, field) \
    if(key==name) return set_value(key,value,object.field,parse_double,error)
#define SET_BOOL(name, field) \
    if(key==name) return set_value(key,value,object.field,parse_bool,error)

    SET_BOOL("rigid",rigid);
    SET_BOOL("material_enabled",material_enabled);
    SET_BOOL("tracked",tracked);
    SET_DOUBLE("center_x",center_x);
    SET_DOUBLE("center_y",center_y);
    SET_DOUBLE("radius",radius);
    SET_DOUBLE("half_width",half_width);
    SET_DOUBLE("half_height",half_height);
    SET_DOUBLE("rotation",rotation);
    SET_DOUBLE("gravity",gravity);
    SET_DOUBLE("velocity_x",velocity_x);
    SET_DOUBLE("velocity_y",velocity_y);
    SET_DOUBLE("angular_velocity",angular_velocity);
    SET_DOUBLE("rigid_offset",rigid_offset);
    SET_DOUBLE("shear_modulus",shear_modulus);
    SET_DOUBLE("solid_density",solid_density);
    SET_BOOL("set_velocity",set_velocity);
    SET_DOUBLE("spin_scale",spin_scale);
    SET_DOUBLE("spin_a1",spin_a1);
    SET_DOUBLE("spin_a2",spin_a2);
    SET_DOUBLE("spin_pivot_radius",spin_pivot_radius);
    SET_DOUBLE("spin_max_angle",spin_max_angle);
    SET_DOUBLE("spin_angular_velocity",spin_angular_velocity);

#undef SET_DOUBLE
#undef SET_BOOL

    if(key=="shape") {
        if(value!="circle"&&value!="square"&&value!="rectangle"&&
           value!="triangle"&&value!="right_triangle"&&
           value!="spinner") {
            error="unknown object shape: " + value;
            return false;
        }
        object.shape=value;
        return true;
    }
    error="unknown object key: " + key;
    return false;
}

}

object_config::object_config() :
    rigid(false),material_enabled(true),tracked(false),
    center_x(0.),center_y(0.),radius(0.25),half_width(0.5),half_height(0.5),
    rotation(0.),gravity(0.),velocity_x(0.),velocity_y(0.),
    angular_velocity(0.),rigid_offset(0.),
    shear_modulus(24.),solid_density(4.),set_velocity(true),
    spin_scale(0.8),spin_a1(0.5),spin_a2(0.125),spin_pivot_radius(0.2),
    spin_max_angle(3.141592653589793),spin_angular_velocity(1.) {}

simulation_config default_config() {
    simulation_config config;
    config.grid_resolution=128;
    config.grid_aspect_ratio=1;
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
    config.reynolds_number=0.;
    config.flow_speed=1.;
    config.flow_type="zero";
    config.extra_viscosity_multiplier=0.8;
    config.transition_viscosity_multiplier=1.;
    config.timestep_padding=0.4;
    config.viscosity_timestep_padding=0.8;
    config.set_velocity=true;
    config.solid_periodic_boundary=false;
    config.output_name="simulation";
    config.domain_x_min=-1.;
    config.domain_x_max=1.;
    config.domain_y_min=-1.;
    config.domain_y_max=1.;
    config.periodic_x=false;
    config.periodic_y=false;
    return config;
}

bool load_config(const char *filename,simulation_config &config,
                 std::string &error) {
    std::ifstream input(filename);
    if(!input) {
        error="could not open configuration file: " + std::string(filename);
        return false;
    }

    enum section_type { NO_SECTION, SIMULATION_SECTION, OBJECT_SECTION };
    section_type section=NO_SECTION;
    bool simulation_seen=false;
    std::string line;
    int line_number=0;
    while(std::getline(input,line)) {
        line_number++;
        std::string::size_type comment=line.find('#');
        if(comment!=std::string::npos) line.erase(comment);
        line=trim(line);
        if(line.empty()) continue;

        if(line=="[simulation]") {
            if(simulation_seen||!config.objects.empty()) {
                error="the [simulation] section must appear once before objects";
            } else {
                simulation_seen=true;
                section=SIMULATION_SECTION;
            }
            if(!error.empty()) {
                std::ostringstream message;
                message<<"line "<<line_number<<": "<<error;
                error=message.str();
                return false;
            }
            continue;
        }
        if(line=="[object]") {
            if(!simulation_seen||section==NO_SECTION) {
                error="the [simulation] section must precede every [object]";
                std::ostringstream message;
                message<<"line "<<line_number<<": "<<error;
                error=message.str();
                return false;
            }
            config.objects.push_back(object_config());
            object_config &object=config.objects.back();
            object.shear_modulus=config.shear_modulus;
            object.solid_density=config.solid_density;
            object.set_velocity=config.set_velocity;
            section=OBJECT_SECTION;
            continue;
        }

        std::string::size_type separator=line.find('=');
        if(separator==std::string::npos) {
            std::ostringstream message;
            message<<"line "<<line_number<<" is missing '='";
            error=message.str();
            return false;
        }
        std::string key=trim(line.substr(0,separator));
        std::string value=trim(line.substr(separator+1));
        if(section==NO_SECTION||key.empty()||value.empty()) {
            std::ostringstream message;
            message<<"line "<<line_number<<" has a key/value outside a section or an empty key/value";
            error=message.str();
            return false;
        }
        bool ok=section==SIMULATION_SECTION?
            set_simulation_value(key,value,config,error):
            set_object_value(key,value,config.objects.back(),error);
        if(!ok) {
            std::ostringstream message;
            message<<"line "<<line_number<<": "<<error;
            error=message.str();
            return false;
        }
    }
    if(!simulation_seen) {
        error="configuration is missing the [simulation] section";
        return false;
    }
    return validate_config(config,error);
}

bool validate_config(const simulation_config &config,std::string &error) {
    if(config.grid_resolution<20||config.grid_resolution>65536)
        error="grid_resolution must be between 20 and 65536";
    else if(config.grid_aspect_ratio<1||
            config.grid_resolution>65536/config.grid_aspect_ratio)
        error="grid aspect ratio produces an unsupported grid";
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
    else if(config.reynolds_number<0)
        error="reynolds_number cannot be negative";
    else if(config.extra_viscosity_multiplier<=0||
            config.transition_viscosity_multiplier<=0||
            config.timestep_padding<=0||
            config.viscosity_timestep_padding<=0)
        error="viscosity and timestep multipliers must be positive";
    else if(config.output_name.empty())
        error="output_name cannot be empty";
    else if(config.domain_x_min>=config.domain_x_max||
            config.domain_y_min>=config.domain_y_max)
        error="domain minimums must be less than maximums";
    else {
        for(unsigned int i=0;i<config.objects.size();i++) {
            const object_config &object=config.objects[i];
            if(object.shape.empty()) {
                error="every [object] section must specify shape";
                return false;
            }
            if(object.material_enabled&&
               (object.shear_modulus<=0||object.solid_density<=0)) {
                error="object material properties must be positive";
                return false;
            }
            if(object.rigid&&object.shape=="spinner") {
                error="spinner cannot be used as a rigid mask; add a rigid geometry object";
                return false;
            }
            if(!object.material_enabled&&!object.rigid) {
                error="an object without material must define a rigid mask";
                return false;
            }
            if(object.tracked&&object.shape!="circle") {
                error="tracked is supported only for circle objects";
                return false;
            }
            if(object.tracked&&object.gravity!=0.) {
                error="tracked circles do not support gravity";
                return false;
            }
            if(object.gravity!=0.&&object.shape!="circle"&&
               object.shape!="square"&&object.shape!="rectangle") {
                error="gravity is supported only for circles and box objects";
                return false;
            }
            if((object.shape=="circle"||object.shape=="square"||
                object.shape=="triangle")&&object.radius<=0) {
                error="circle, square, and triangle radii must be positive";
                return false;
            }
            if((object.shape=="rectangle"||
                object.shape=="right_triangle")&&
               (object.half_width<=0||object.half_height<=0)) {
                error="object half-width and half-height must be positive";
                return false;
            }
            if(object.shape=="spinner"&&
               (object.spin_scale<=0||object.spin_pivot_radius<=0)) {
                error="spinner scale and pivot radius must be positive";
                return false;
            }
        }
        return true;
    }
    return false;
}

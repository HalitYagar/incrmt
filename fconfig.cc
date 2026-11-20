#include <cstdio>
#include <sys/stat.h>
#include <sstream>
#include <vector>

#include "config.hh"
#include "obj_field.hh"
#include "fluid_2d.hh"

int main(int argc,char **argv) {

    if(argc!=2&&argc!=3) {
        fputs("Syntax: ./fconfig <configuration file> [output directory]\n",stderr);
        return 1;
    }

    simulation_config config=default_config();
    std::string error;
    if(!load_config(argv[1],config,error)) {
        fprintf(stderr,"Configuration error: %s\n",error.c_str());
        return 1;
    }

    int m=config.grid_resolution,n=m;
    const char *output_dir=argc==3?argv[2]: "output";
    mkdir(output_dir,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

    std::ostringstream run_name;
    run_name<<output_dir<<"/"<<config.output_name;

    std::string fn=run_name.str();

    char *fout=new char[fn.size()+5];
    sprintf(fout,"%s.odr",fn.c_str());
    if(mkdir(fout,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)!=0) {
        delete [] fout;
        fprintf(stderr,"Could not create output directory for this run\n");
        return 1;
    }

    const char *filename=fn.c_str();
    std::vector<object*> obj;

    // Create object and set non-periodic boundary conditions
    obj.push_back(new obj_circle_track(config.circle_x,config.circle_y,
                                       config.circle_radius,
                                       config.circle_velocity_x,
                                       config.circle_velocity_y,
                                       config.circle_angular_velocity,filename));
    sim_type sim_t(config.domain_x_min,config.domain_x_max,
                   config.domain_y_min,config.domain_y_max,
                   config.periodic_x,config.periodic_y);
    

    // Create a directory for output if one hasn't been created yet
    // Construct the simulation class and add objects
    fluid_2d f2d(m,n,sim_t,config.viscosity,config.fluid_density,
                config.transition_width,config.pinning_width,
                config.num_tracers,config.output_flags,fout);
    mat_const mc(config.shear_modulus,config.solid_density,
                 config.transition_viscosity_multiplier,config.set_velocity);
    for(unsigned int k=0;k<obj.size();k++) f2d.add_object(obj[k],mc);

    // Initialize the simulation and set the extra viscosity and timestep based
    // on the padding factors
    f2d.initialize(config.extra_viscosity_multiplier,
                   config.timestep_padding,
                   config.viscosity_timestep_padding);

    // Run the simulation
    f2d.solve(config.duration,config.num_frames);

    // Free dynamically allocated objects
    for(unsigned int k=0;k<obj.size();k++) delete obj[k];
    delete [] fout;
}

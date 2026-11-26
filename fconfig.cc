#include <cstdio>
#include <sys/stat.h>
#include <sstream>
#include <vector>
#include <cmath>

#include "config.hh"
#include "obj_field.hh"
#include "fluid_2d.hh"
#include "sim_type.hh"
#include "examples/config_rect.hh"
#include "examples/obj_rect_ellipse.hh"
#include "examples/obj_csg_shape.hh"

struct configured_object {
    object *shape;
    mat_const material;
    configured_object(object *shape_,const mat_const &material_) :
        shape(shape_),material(material_) {}
};

struct rigid_mask {
    obj_standard *shape;
    double offset;
    rigid_mask(obj_standard *shape_,double offset_) :
        shape(shape_),offset(offset_) {}
};

static void register_object(std::vector<object*> &owned,
                            std::vector<configured_object> &objects,
                            object *shape,const object_config &config,
                            double transition_viscosity_multiplier) {
    owned.push_back(shape);
    if(config.material_enabled) {
        mat_const material(config.shear_modulus,config.solid_density,
                           transition_viscosity_multiplier,
                           config.set_velocity);
        objects.push_back(configured_object(shape,material));
    }
}

static object *make_object(const object_config &config,const char *filename,
                           std::vector<object*> &owned,
                           obj_standard *&mask_shape) {
    mask_shape=NULL;
    object *shape=NULL;
    if(config.shape=="circle") {
        if(config.tracked) {
            shape=new obj_circle_track(config.center_x,config.center_y,
                config.radius,config.velocity_x,config.velocity_y,
                config.angular_velocity,filename);
            if(config.rigid) {
                mask_shape=new obj_circle(config.center_x,config.center_y,
                                          config.radius,config.gravity);
                owned.push_back(mask_shape);
            }
        } else {
            shape=new obj_circle(config.center_x,config.center_y,
                config.radius,config.velocity_x,config.velocity_y,
                config.angular_velocity,config.gravity);
        }
    } else if(config.shape=="square") {
        shape=new configured_rect(config.center_x,config.center_y,
            config.radius,config.radius,config.rotation,config.velocity_x,
            config.velocity_y,config.angular_velocity,config.gravity);
    } else if(config.shape=="rectangle") {
        shape=new configured_rect(config.center_x,config.center_y,
            config.half_width,config.half_height,config.rotation,
            config.velocity_x,config.velocity_y,config.angular_velocity,
            config.gravity);
    } else if(config.shape=="triangle") {
        shape=new obj_triangle(config.center_x,config.center_y,config.radius,
            config.rotation,config.gravity,config.velocity_x,
            config.velocity_y,config.angular_velocity);
    } else if(config.shape=="rect_ellipse") {
        shape=new obj_rect_ellipse(config.center_x,config.center_y,
            config.half_width,config.half_height,config.ellipse_offset_x,
            config.ellipse_offset_y,config.ellipse_radius_x,
            config.ellipse_radius_y,config.velocity_x,config.velocity_y,
            config.angular_velocity);
    } else if(config.shape=="csg"||config.shape=="ellipse") {
        csg_op operation=CSG_ELLIPSE_ONLY;
        if(config.shape=="csg")
            csg_op_parse(config.shape_operation.c_str(),operation);
        shape=new obj_csg_shape(config.center_x,config.center_y,
            config.half_width,config.half_height,config.ellipse_offset_x,
            config.ellipse_offset_y,config.ellipse_radius_x,
            config.ellipse_radius_y,operation,config.shape_blend,
            config.velocity_x,config.velocity_y,config.angular_velocity);
    } else if(config.shape=="spinner") {
        shape=new obj_simple_spin(config.spin_scale,config.spin_a1,
            config.spin_a2,config.spin_pivot_radius,config.spin_max_angle,
            config.spin_angular_velocity,filename);
    }

    if(mask_shape==NULL&&config.rigid)
        mask_shape=dynamic_cast<obj_standard*>(shape);
    return shape;
}

static void mark_rigid_shape(fluid_2d &f2d,int m,int n,
                             double ax,double bx,double ay,double by,
                             obj_standard &shape,double offset) {
    for(int j=0;j<n;j++) {
        double y=ay+(j+0.5)*(by-ay)/n;
        for(int i=0;i<m;i++) {
            double x=ax+(i+0.5)*(bx-ax)/m;
            if(shape.phi(x-shape.cx,y-shape.cy)<offset)
                f2d.rigid[i+m*j]=true;
        }
    }
}

int main(int argc,char **argv) {
    if(argc!=2&&argc!=3) {
        fputs("Syntax: ./fconfig <configuration file> [output directory]\n"
              "       ./fconfig --validate <configuration file>\n",stderr);
        return 1;
    }

    bool validate_only=argc==3&&std::string(argv[1])=="--validate";
    simulation_config config=default_config();
    std::string error;
    const char *config_file=validate_only?argv[2]:argv[1];
    if(!load_config(config_file,config,error)) {
        fprintf(stderr,"Configuration error: %s\n",error.c_str());
        return 1;
    }
    if(validate_only) {
        printf("Configuration is valid: %s\n",config_file);
        return 0;
    }

    if(config.reynolds_number>0)
        config.viscosity=config.fluid_density*0.5/config.reynolds_number;

    int n=config.grid_resolution;
    int m=n*config.grid_aspect_ratio;
    const char *output_dir=argc==3?argv[2]:"output";
    mkdir(output_dir,S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH);

    std::ostringstream run_name;
    run_name<<output_dir<<"/"<<config.output_name;
    std::string filename=run_name.str();
    std::string output_path=filename+".odr";
    if(mkdir(output_path.c_str(),S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH)!=0) {
        fprintf(stderr,"Could not create output directory for this run\n");
        return 1;
    }

    std::vector<object*> owned_objects;
    std::vector<configured_object> objects;
    std::vector<rigid_mask> rigid_masks;
    for(unsigned int i=0;i<config.objects.size();i++) {
        const object_config &object_config=config.objects[i];
        obj_standard *mask_shape=NULL;
        object *shape=make_object(object_config,filename.c_str(),
                                  owned_objects,mask_shape);
        register_object(owned_objects,objects,shape,object_config,
                        config.transition_viscosity_multiplier);
        if(object_config.rigid) {
            if(mask_shape==NULL) {
                fprintf(stderr,"Configuration error: object %u cannot define a rigid mask\n",i+1);
                for(unsigned int k=0;k<owned_objects.size();k++)
                    delete owned_objects[k];
                return 1;
            }
            rigid_masks.push_back(rigid_mask(mask_shape,
                                             object_config.rigid_offset));
        }
    }

    sim_type *sim_t;
    if(config.flow_type=="horizontal")
        sim_t=new sim_horiz_flow(config.flow_speed,config.domain_x_min,
            config.domain_x_max,config.domain_y_min,config.domain_y_max);
    else
        sim_t=new sim_type(config.domain_x_min,config.domain_x_max,
            config.domain_y_min,config.domain_y_max,config.periodic_x,
            config.periodic_y);

    {
        fluid_2d f2d(m,n,*sim_t,config.viscosity,config.fluid_density,
            config.transition_width,config.pinning_width,config.num_tracers,
            config.output_flags,output_path.c_str());
        f2d.solid_prd_bc=config.solid_periodic_boundary;
        for(unsigned int i=0;i<objects.size();i++)
            f2d.add_object(objects[i].shape,objects[i].material);

        if(!rigid_masks.empty()) {
            f2d.init_rigid_objects();
            for(unsigned int i=0;i<rigid_masks.size();i++)
                mark_rigid_shape(f2d,m,n,config.domain_x_min,
                    config.domain_x_max,config.domain_y_min,
                    config.domain_y_max,*rigid_masks[i].shape,
                    rigid_masks[i].offset);
        }

        f2d.initialize(config.extra_viscosity_multiplier,
                       config.timestep_padding,
                       config.viscosity_timestep_padding);
        f2d.solve(config.duration,config.num_frames);
    }

    for(unsigned int i=0;i<owned_objects.size();i++)
        delete owned_objects[i];
    delete sim_t;
}

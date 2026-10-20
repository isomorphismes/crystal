#include "crystal_growth.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define GROWTH_PI 3.14159265358979323846
static GrowthVector add(GrowthVector a,GrowthVector b) { return (GrowthVector){a.x+b.x,a.y+b.y,a.z+b.z}; }
static GrowthVector sub(GrowthVector a,GrowthVector b) { return (GrowthVector){a.x-b.x,a.y-b.y,a.z-b.z}; }
static GrowthVector scale(GrowthVector a,double s) { return (GrowthVector){a.x*s,a.y*s,a.z*s}; }
static double dot(GrowthVector a,GrowthVector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static GrowthVector cross(GrowthVector a,GrowthVector b) { return (GrowthVector){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
static double length(GrowthVector a) { return sqrt(dot(a,a)); }
static GrowthVector unit(GrowthVector a) { double n=length(a); return n>0?scale(a,1/n):(GrowthVector){0}; }
static bool finite_vector(GrowthVector a) { return isfinite(a.x)&&isfinite(a.y)&&isfinite(a.z); }
static GrowthVector world(const GrowthLaw *law,GrowthVector a) {
    const double *r=law->frame;
    return (GrowthVector){r[0]*a.x+r[1]*a.y+r[2]*a.z,r[3]*a.x+r[4]*a.y+r[5]*a.z,r[6]*a.x+r[7]*a.y+r[8]*a.z};
}
static GrowthVector material(const GrowthLaw *law,GrowthVector a) {
    const double *r=law->frame;
    return (GrowthVector){r[0]*a.x+r[3]*a.y+r[6]*a.z,r[1]*a.x+r[4]*a.y+r[7]*a.z,r[2]*a.x+r[5]*a.y+r[8]*a.z};
}
GrowthLaw growth_default_law(GrowthMaterial material_kind) {
    GrowthLaw law={0};
    law.material=material_kind; law.baseline=1; law.seed_radius=.12;
    law.frame[0]=law.frame[4]=law.frame[8]=1; law.handedness=1;
    law.redistribution=.002;
    const double defaults[4][3]={{.24,.66,.92},{.30,.36,.43},{.34,.42,.55},{.29,.33,.47}};
    if (material_kind>=GROWTH_HALITE && material_kind<=GROWTH_PYRITE)
        memcpy(law.speed,defaults[material_kind],sizeof(law.speed));
    law.hopper=material_kind==GROWTH_BISMUTH;
    return law;
}
static bool valid_law(const GrowthLaw *law) {
    if (!law || law->material<GROWTH_HALITE || law->material>GROWTH_PYRITE ||
        (law->handedness!=1 && law->handedness!=-1) ||
        !isfinite(law->seed_radius) || law->seed_radius<.001 || law->seed_radius>10 ||
        !isfinite(law->baseline) || law->baseline<=0 || law->baseline>10 ||
        !isfinite(law->redistribution) || law->redistribution<0 || law->redistribution>.01) return false;
    for(int i=0;i<3;i++) if(!isfinite(law->speed[i]) || law->speed[i]<=0 || law->speed[i]>10) return false;
    for(int i=0;i<9;i++) if(!isfinite(law->frame[i])) return false;
    GrowthVector axes[3]={world(law,(GrowthVector){1,0,0}),world(law,(GrowthVector){0,1,0}),world(law,(GrowthVector){0,0,1})};
    for(int i=0;i<3;i++) for(int j=0;j<3;j++) if(fabs(dot(axes[i],axes[j])-(i==j))>1e-9) return false;
    return dot(cross(axes[0],axes[1]),axes[2])>0;
}
static bool add_plane(CrystalGrowth *state,GrowthVector normal,int family) {
    normal=unit(normal);
    for(int i=0;i<state->plane_count;i++) if(dot(normal,state->planes[i].normal)>1-1e-12) return true;
    if(state->plane_count>=GROWTH_PLANES) return false;
    state->planes[state->plane_count++]=(GrowthPlane){normal,state->law.speed[family],family};
    return true;
}
/* Proper D3 orbit: three turns about y and a half-turn about x. Inversion is
 * added only for centrosymmetric A7, never for quartz class 32. */
static bool trigonal_orbit(CrystalGrowth *state,double angle,double height,int family,bool inversion) {
    for(int turn=0;turn<3;turn++) for(int half=0;half<2;half++) {
        double phase=(half?-angle:angle)+turn*2*GROWTH_PI/3;
        GrowthVector normal={cos(phase),half?-height:height,sin(phase)};
        if(!add_plane(state,normal,family)) return false;
        if(inversion && !add_plane(state,scale(normal,-1),family)) return false;
    }
    return true;
}
static bool configure_planes(CrystalGrowth *state) {
    GrowthMaterial kind=state->law.material;
    if(kind==GROWTH_HALITE || kind==GROWTH_PYRITE) {
        for(int x=-1;x<=1;x++) for(int y=-1;y<=1;y++) for(int z=-1;z<=1;z++) {
            int nonzero=(x!=0)+(y!=0)+(z!=0);
            if(nonzero==1 && !add_plane(state,(GrowthVector){x,y,z},0)) return false;
            if(nonzero==3 && !add_plane(state,(GrowthVector){x,y,z},kind==GROWTH_HALITE?1:2)) return false;
            if(nonzero==2 && kind==GROWTH_HALITE && !add_plane(state,(GrowthVector){x,y,z},2)) return false;
        }
        if(kind==GROWTH_PYRITE) {
            /* Even cyclic permutations and independent signs: 12 faces,
             * not the 24-face m-3m orbit that would erase diploidal symmetry. */
            for(int axis=0;axis<3;axis++) for(int first=-1;first<=1;first+=2) for(int second=-1;second<=1;second+=2) {
                double p[3]={0}; p[axis]=2*first; p[(axis+1)%3]=second;
                if(!add_plane(state,(GrowthVector){p[0],p[1],p[2]},1)) return false;
            }
        }
    } else if(kind==GROWTH_QUARTZ) {
        double phase=state->law.handedness*.12;
        if(!trigonal_orbit(state,GROWTH_PI/6,0,0,false) ||
           !trigonal_orbit(state,phase,1,1,false) ||
           !trigonal_orbit(state,GROWTH_PI/3+phase,1,2,false)) return false;
    } else {
        if(!add_plane(state,(GrowthVector){0,1,0},0) || !add_plane(state,(GrowthVector){0,-1,0},0) ||
           !trigonal_orbit(state,GROWTH_PI/6,.85,1,true) ||
           !trigonal_orbit(state,0,.45,2,true)) return false;
    }
    return true;
}
static double radial_limit(const CrystalGrowth *state,GrowthVector direction,double time,int *active,double *next) {
    GrowthVector local=material(&state->law,unit(direction));
    double closest=state->law.seed_radius+state->law.baseline*time;
    double second=closest; int selected=-1;
    for(int plane=0;plane<state->plane_count;plane++) {
        double cosine=dot(local,state->planes[plane].normal);
        if(cosine<=1e-12) continue;
        double distance=(state->law.seed_radius+state->planes[plane].speed*time)/cosine;
        if(distance<closest) { second=closest; closest=distance; selected=plane; }
        else if(distance<second || second==closest) second=distance;
    }
    if(active) *active=selected;
    if(next) *next=second;
    return closest;
}
double growth_front_radius(const CrystalGrowth *state,GrowthVector direction,double time) {
    if(!state || !finite_vector(direction) || length(direction)<1e-12 || !isfinite(time) || time<0) return NAN;
    return radial_limit(state,direction,time,NULL,NULL);
}
static void sample_front(CrystalGrowth *state,const CrystalGrowth *previous) {
    for(int node=0;node<GROWTH_NODES;node++) {
        int active=-1; double second;
        double radius=radial_limit(state,state->direction[node],state->time,&active,&second);
        GrowthVector normal=active<0?state->direction[node]:world(&state->law,state->planes[active].normal);
        state->surface_normal[node]=normal;
        state->normal_speed[node]=active<0?state->law.baseline:state->planes[active].speed;
        if(state->law.hopper && state->law.material==GROWTH_BISMUTH && previous) {
            /* Provisional hopper kinetics: edges outrun face centers; unreleased
             * face advance is retained as credit until a common material-normal
             * terrace level is reached, or the 48-step ledge memory expires.
             * Every node still belongs to the same radial sphere complex. */
            double old=length(previous->position[node]);
            double edge=exp(-25*fmax(0,second-radius)/fmax(radius,.001));
            double factor=.58+(.87*edge);
            double previous_front=radial_limit(previous,state->direction[node],previous->time,NULL,NULL);
            double proposed=fmin(radius,old+fmax(0,radius-previous_front)*factor+previous->terrace_credit[node]);
            state->terrace_credit[node]=0; state->ledge_age[node]=0;
            if(edge<.7 && active>=0) {
                double cosine=dot(state->direction[node],normal);
                double height=proposed*cosine;
                double level=state->law.seed_radius+.08*floor((height-state->law.seed_radius)/.08);
                double retained=fmax(old,fmin(proposed,level/cosine));
                unsigned int age=previous->ledge_age[node]+1;
                if(age<48 && retained<proposed) {
                    state->terrace_credit[node]=proposed-retained; state->ledge_age[node]=age;
                    proposed=retained;
                }
            }
            radius=proposed;
        }
        state->position[node]=scale(state->direction[node],radius);
    }
}
bool growth_initialize(CrystalGrowth *output,const GrowthLaw *law,uint64_t seed) {
    if(!output || !valid_law(law)) return false;
    CrystalGrowth state={0}; state.law=*law; state.seed=seed;
    if(!configure_planes(&state)) return false;
    int coordinates[GROWTH_NODES][3],count=0;
    /* One cubed-sphere seed and one incidence complex for every material.
     * Coordinates only identify shared grid nodes; mineral geometry is absent. */
    for(int x=-2;x<=2;x++) for(int y=-2;y<=2;y++) for(int z=-2;z<=2;z++) {
        if(abs(x)!=2 && abs(y)!=2 && abs(z)!=2) continue;
        coordinates[count][0]=x; coordinates[count][1]=y; coordinates[count][2]=z;
        state.direction[count++]=unit((GrowthVector){x,y,z});
    }
    int faces=0;
    for(int axis=0;axis<3;axis++) for(int side=-2;side<=2;side+=4) for(int u=-2;u<2;u++) for(int v=-2;v<2;v++) {
        int corner[4],du[4]={0,1,1,0},dv[4]={0,0,1,1};
        for(int c=0;c<4;c++) {
            int wanted[3]; wanted[axis]=side; wanted[(axis+1)%3]=u+du[c]; wanted[(axis+2)%3]=v+dv[c];
            corner[c]=-1;
            for(int n=0;n<count;n++) if(coordinates[n][0]==wanted[0] && coordinates[n][1]==wanted[1] && coordinates[n][2]==wanted[2]) corner[c]=n;
            if(corner[c]<0) return false;
        }
        for(int triangle=0;triangle<2;triangle++) {
            GrowthFace face={{corner[0],corner[triangle+1],corner[triangle+2]}};
            if(side<0) { int swap=face.node[1]; face.node[1]=face.node[2]; face.node[2]=swap; }
            state.face[faces++]=face;
        }
    }
    if(count!=GROWTH_NODES || faces!=GROWTH_FACES) return false;
    sample_front(&state,NULL);
    if(!growth_validate(&state,NULL)) return false;
    *output=state; return true;
}
bool growth_validate(const CrystalGrowth *state,GrowthMetrics *output) {
    if(!state || !valid_law(&state->law) || !isfinite(state->time) || state->time<0 || state->time>10000 ||
       state->plane_count<1 || state->plane_count>GROWTH_PLANES) return false;
    GrowthMetrics metrics={0}; metrics.minimum_radius=1e100; metrics.minimum_solid_angle=1e100;
    for(int plane=0;plane<state->plane_count;plane++) {
        if(!finite_vector(state->planes[plane].normal) || fabs(length(state->planes[plane].normal)-1)>1e-9 ||
           !isfinite(state->planes[plane].speed) || state->planes[plane].speed<=0 ||
           state->planes[plane].family<0 || state->planes[plane].family>2) return false;
    }
    metrics.vertices=GROWTH_NODES; metrics.faces=GROWTH_FACES;
    unsigned char edge_count[GROWTH_NODES][GROWTH_NODES]={{0}};
    int edge_direction[GROWTH_NODES][GROWTH_NODES]={{0}};
    int used[GROWTH_NODES]={0}; double solid_angle=0;
    for(int n=0;n<GROWTH_NODES;n++) {
        if(!finite_vector(state->position[n]) || !finite_vector(state->direction[n]) ||
           fabs(length(state->direction[n])-1)>1e-8 || !finite_vector(state->surface_normal[n]) ||
           fabs(length(state->surface_normal[n])-1)>1e-8 || !isfinite(state->normal_speed[n]) || state->normal_speed[n]<=0 ||
           !isfinite(state->terrace_credit[n]) || state->terrace_credit[n]<0 || state->ledge_age[n]>=48) return false;
        double radius=length(state->position[n]);
        if(radius<1e-6 || !isfinite(radius) || dot(state->position[n],state->direction[n])<=0 ||
           length(cross(state->position[n],state->direction[n]))>1e-8*radius) return false;
        metrics.minimum_radius=fmin(metrics.minimum_radius,radius); metrics.maximum_radius=fmax(metrics.maximum_radius,radius);
    }
    for(int f=0;f<GROWTH_FACES;f++) {
        const int *ids=state->face[f].node;
        for(int j=0;j<3;j++) if(ids[j]<0 || ids[j]>=GROWTH_NODES) return false;
        if(ids[0]==ids[1] || ids[1]==ids[2] || ids[2]==ids[0]) return false;
        GrowthVector a=state->position[ids[0]],b=state->position[ids[1]],c=state->position[ids[2]];
        GrowthVector normal=cross(sub(b,a),sub(c,a));
        double area=length(normal)/2, volume=dot(a,cross(b,c))/6;
        if(!isfinite(area) || area<1e-12 || !isfinite(volume) || volume<=1e-15) return false;
        metrics.area+=area; metrics.volume+=volume;
        GrowthVector da=state->direction[ids[0]],db=state->direction[ids[1]],dc=state->direction[ids[2]];
        double angle=2*atan2(dot(da,cross(db,dc)),1+dot(da,db)+dot(db,dc)+dot(dc,da));
        if(!(angle>1e-8) || !isfinite(angle)) return false;
        solid_angle+=angle; metrics.minimum_solid_angle=fmin(metrics.minimum_solid_angle,angle);
        for(int j=0;j<3;j++) {
            int first=ids[j],second=ids[(j+1)%3]; used[first]=1;
            int low=first<second?first:second,high=first<second?second:first;
            if(++edge_count[low][high]>2) return false;
            edge_direction[low][high]+=(first<second?1:-1);
        }
    }
    for(int a=0;a<GROWTH_NODES;a++) {
        if(!used[a]) return false;
        for(int b=a+1;b<GROWTH_NODES;b++) if(edge_count[a][b]) {
            if(edge_count[a][b]!=2 || edge_direction[a][b]!=0) return false;
            metrics.edges++;
        }
    }
    if(metrics.vertices-metrics.edges+metrics.faces!=2 || fabs(solid_angle-4*GROWTH_PI)>1e-7) return false;
    /* Positive radial faces with total degree one cannot overlap in their
     * interiors: every regular ray has exactly one positively oriented preimage.
     * Check each vertex link is one cycle, excluding pinched pseudomanifolds. */
    for(int vertex=0;vertex<GROWTH_NODES;vertex++) {
        int neighbors[GROWTH_NODES][2],degree[GROWTH_NODES]={0},number=0;
        for(int f=0;f<GROWTH_FACES;f++) for(int j=0;j<3;j++) if(state->face[f].node[j]==vertex) {
            int a=state->face[f].node[(j+1)%3],b=state->face[f].node[(j+2)%3];
            if(degree[a]>=2 || degree[b]>=2) return false;
            neighbors[a][degree[a]++]=b; neighbors[b][degree[b]++]=a;
        }
        int start=-1;
        for(int n=0;n<GROWTH_NODES;n++) if(degree[n]) { if(degree[n]!=2) return false; start=n; number++; }
        if(start<0) return false;
        int previous=-1,current=start,walked=0;
        do { int next=neighbors[current][0]==previous?neighbors[current][1]:neighbors[current][0]; previous=current; current=next; if(++walked>number) return false; } while(current!=start);
        if(walked!=number) return false;
    }
    if(output) *output=metrics;
    return true;
}
static void redistribute(CrystalGrowth *next) {
    if(next->law.redistribution==0) return;
    GrowthVector displacement[GROWTH_NODES]={{0}}; int degree[GROWTH_NODES]={0};
    for(int f=0;f<GROWTH_FACES;f++) for(int j=0;j<3;j++) {
        int a=next->face[f].node[j],b=next->face[f].node[(j+1)%3];
        GrowthVector delta=sub(next->direction[b],next->direction[a]);
        displacement[a]=add(displacement[a],delta); displacement[b]=sub(displacement[b],delta); degree[a]++; degree[b]++;
    }
    for(int n=0;n<GROWTH_NODES;n++) {
        GrowthVector radial=next->direction[n];
        GrowthVector tangent=sub(displacement[n],scale(radial,dot(displacement[n],radial)));
        /* Curvature monitor: adjacent material normals disagree most near a
         * facet boundary. Limit tangential relaxation there to retain samples. */
        double second; int active;
        double radius=radial_limit(next,radial,next->time,&active,&second);
        double monitor=1+8*exp(-20*fmax(0,second-radius)/radius);
        GrowthVector shift=scale(tangent,next->law.redistribution/(degree[n]*monitor));
        if(active>=0) {
            GrowthVector local=material(&next->law,radial);
            int competitor=-1; double competing_radius=1e100;
            for(int plane=0;plane<next->plane_count;plane++) {
                double cosine=dot(local,next->planes[plane].normal);
                if(plane==active || cosine<=1e-8) continue;
                double distance=(next->law.seed_radius+next->planes[plane].speed*next->time)/cosine;
                if(distance<competing_radius) { competing_radius=distance; competitor=plane; }
            }
            if(competitor>=0) {
                GrowthVector first_normal=world(&next->law,next->planes[active].normal);
                GrowthVector second_normal=world(&next->law,next->planes[competitor].normal);
                GrowthVector gradient=sub(scale(first_normal,radius/dot(radial,first_normal)),
                                          scale(second_normal,competing_radius/dot(radial,second_normal)));
                gradient=sub(gradient,scale(radial,dot(gradient,radial)));
                double squared=dot(gradient,gradient),gap=competing_radius-radius;
                if(squared>1e-12 && gap>1e-9 && gap<.2*radius) {
                    /* Attract toward the nearest material boundary, without
                     * consuming the whole gap or changing the node budget. */
                    double amount=fmin(.25*gap/squared,next->law.redistribution*.05/sqrt(squared));
                    shift=sub(shift,scale(gradient,amount));
                }
            }
        }
        next->direction[n]=unit(add(radial,shift));
    }
}
static double triangle_quality(const CrystalGrowth *state,GrowthFace face) {
    GrowthVector a=state->direction[face.node[0]],b=state->direction[face.node[1]],c=state->direction[face.node[2]];
    double determinant=dot(a,cross(b,c));
    double squared=dot(sub(a,b),sub(a,b))+dot(sub(b,c),sub(b,c))+dot(sub(c,a),sub(c,a));
    return determinant>1e-8?determinant/squared:0;
}
static void improve_one_edge(CrystalGrowth *state) {
    for(int first=0;first<GROWTH_FACES;first++) for(int side=0;side<3;side++) {
        int a=state->face[first].node[side],b=state->face[first].node[(side+1)%3],c=state->face[first].node[(side+2)%3];
        for(int second=first+1;second<GROWTH_FACES;second++) for(int edge=0;edge<3;edge++) {
            if(state->face[second].node[edge]!=b || state->face[second].node[(edge+1)%3]!=a) continue;
            int d=state->face[second].node[(edge+2)%3]; bool exists=false;
            for(int f=0;f<GROWTH_FACES;f++) for(int e=0;e<3;e++) {
                int x=state->face[f].node[e],y=state->face[f].node[(e+1)%3];
                if((x==c && y==d)||(x==d && y==c)) exists=true;
            }
            if(exists) continue;
            GrowthFace replacement_first={{c,d,b}},replacement_second={{d,c,a}};
            double before=fmin(triangle_quality(state,state->face[first]),triangle_quality(state,state->face[second]));
            double after=fmin(triangle_quality(state,replacement_first),triangle_quality(state,replacement_second));
            if(after>before*1.01) {
                state->face[first]=replacement_first; state->face[second]=replacement_second; return;
            }
        }
    }
}
bool growth_step(CrystalGrowth *state,double delta_time) {
    if(!state || !isfinite(delta_time) || delta_time<0 || delta_time>.25 || !growth_validate(state,NULL)) return false;
    if(state->paused || delta_time==0) return true;
    CrystalGrowth candidate=*state;
    candidate.time+=delta_time; candidate.steps++;
    redistribute(&candidate);
    if(candidate.steps%64==0) improve_one_edge(&candidate);
    sample_front(&candidate,state);
    if(!growth_validate(&candidate,NULL)) {
        /* Reject only the proposed remeshing when it degenerates. The exact
         * front still advances on the preceding valid sampling complex. */
        memcpy(candidate.direction,state->direction,sizeof(candidate.direction));
        memcpy(candidate.face,state->face,sizeof(candidate.face));
        sample_front(&candidate,state);
        if(!growth_validate(&candidate,NULL)) return false;
    }
    *state=candidate; return true;
}
/* A bounded, versioned text checkpoint. Decimal round trips retain every
 * binary64 state value. Loading is transactional and validates the whole net. */
bool growth_save(FILE *file,const CrystalGrowth *state) {
    if(!file || !growth_validate(state,NULL)) return false;
    if(fprintf(file,"CRYSTAL_GROWTH_2\n%d %d %d %d %.17g %.17g %.17g %.17g %llu %llu\n",
       state->law.material,state->law.handedness,state->law.hopper,state->paused,
       state->law.baseline,state->law.seed_radius,state->law.redistribution,state->time,
       (unsigned long long)state->seed,(unsigned long long)state->steps)<0) return false;
    for(int i=0;i<3;i++) if(fprintf(file,"%.17g\n",state->law.speed[i])<0) return false;
    for(int i=0;i<9;i++) if(fprintf(file,"%.17g\n",state->law.frame[i])<0) return false;
    for(int n=0;n<GROWTH_NODES;n++) {
        GrowthVector d=state->direction[n],p=state->position[n],normal=state->surface_normal[n];
        if(fprintf(file,"%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %u\n",d.x,d.y,d.z,p.x,p.y,p.z,normal.x,normal.y,normal.z,state->normal_speed[n],state->terrace_credit[n],state->ledge_age[n])<0) return false;
    }
    for(int f=0;f<GROWTH_FACES;f++) if(fprintf(file,"%d %d %d\n",state->face[f].node[0],state->face[f].node[1],state->face[f].node[2])<0) return false;
    return !ferror(file);
}
bool growth_load(FILE *file,CrystalGrowth *state) {
    if(!file || !state) return false;
    char magic[32]; int kind,handedness,hopper,paused;
    double baseline,seed_radius,redistribution,time;
    unsigned long long seed,steps;
    if(fscanf(file,"%31s",magic)!=1 || strcmp(magic,"CRYSTAL_GROWTH_2") ||
       fscanf(file,"%d %d %d %d %lf %lf %lf %lf %llu %llu",&kind,&handedness,&hopper,&paused,&baseline,&seed_radius,&redistribution,&time,&seed,&steps)!=10 ||
       (hopper!=0 && hopper!=1) || (paused!=0 && paused!=1)) return false;
    GrowthLaw law=growth_default_law((GrowthMaterial)kind);
    law.handedness=handedness; law.hopper=hopper; law.baseline=baseline; law.seed_radius=seed_radius; law.redistribution=redistribution;
    for(int i=0;i<3;i++) if(fscanf(file,"%lf",&law.speed[i])!=1) return false;
    for(int i=0;i<9;i++) if(fscanf(file,"%lf",&law.frame[i])!=1) return false;
    CrystalGrowth candidate;
    if(!growth_initialize(&candidate,&law,(uint64_t)seed)) return false;
    candidate.time=time; candidate.steps=(uint64_t)steps; candidate.paused=paused;
    for(int n=0;n<GROWTH_NODES;n++) {
        GrowthVector *d=&candidate.direction[n],*p=&candidate.position[n],*normal=&candidate.surface_normal[n];
        if(fscanf(file,"%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %u",&d->x,&d->y,&d->z,&p->x,&p->y,&p->z,&normal->x,&normal->y,&normal->z,&candidate.normal_speed[n],&candidate.terrace_credit[n],&candidate.ledge_age[n])!=12) return false;
    }
    for(int f=0;f<GROWTH_FACES;f++) if(fscanf(file,"%d %d %d",&candidate.face[f].node[0],&candidate.face[f].node[1],&candidate.face[f].node[2])!=3) return false;
    if(!growth_validate(&candidate,NULL)) return false;
    *state=candidate; return true;
}

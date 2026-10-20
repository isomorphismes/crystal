#include "../app/src/main/cpp/crystal_growth.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define PI 3.14159265358979323846
static CrystalGrowth state,replay,mutant;
static double distance(GrowthVector a,GrowthVector b) { return sqrt((a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)+(a.z-b.z)*(a.z-b.z)); }
static GrowthVector turn(GrowthVector a) { double c=cos(2*PI/3),s=sin(2*PI/3); return (GrowthVector){c*a.x-s*a.z,a.y,s*a.x+c*a.z}; }
static void advance(CrystalGrowth *crystal,int steps) { for(int i=0;i<steps;i++) assert(growth_step(crystal,.02)); }
static void law_symmetry(GrowthMaterial kind) {
    GrowthLaw law=growth_default_law(kind); assert(growth_initialize(&state,&law,42));
    for(int i=1;i<=100;i++) {
        GrowthVector direction={sin(i*.7),cos(i*.4),sin(i*.9)};
        double expected=growth_front_radius(&state,direction,4);
        GrowthVector symmetric;
        if(kind==GROWTH_HALITE) symmetric=(GrowthVector){-direction.z,direction.x,direction.y};
        else if(kind==GROWTH_PYRITE) symmetric=(GrowthVector){direction.z,direction.x,-direction.y};
        else symmetric=turn(direction);
        assert(fabs(expected-growth_front_radius(&state,symmetric,4))<1e-11);
        if(kind==GROWTH_QUARTZ || kind==GROWTH_BISMUTH) {
            symmetric=(GrowthVector){direction.x,-direction.y,-direction.z};
            assert(fabs(expected-growth_front_radius(&state,symmetric,4))<1e-11);
        }
        if(kind==GROWTH_BISMUTH || kind==GROWTH_PYRITE) {
            symmetric=(GrowthVector){-direction.x,-direction.y,-direction.z};
            assert(fabs(expected-growth_front_radius(&state,symmetric,4))<1e-11);
        }
    }
}
int main(int argc,char **argv) {
    int steps=argc>1?atoi(argv[1]):400;
    assert(steps>=1 && steps<=10000);
    puts("material\tsteps\ttime\tvertices\tedges\tfaces\tseed_volume\tvolume\tarea\tmin_radius\tmax_radius");
    for(int kind=0;kind<4;kind++) {
        GrowthLaw law=growth_default_law((GrowthMaterial)kind);
        law_symmetry((GrowthMaterial)kind);
        assert(growth_initialize(&state,&law,42)); replay=state;
        GrowthMetrics seed,metrics; assert(growth_validate(&state,&seed));
        advance(&state,steps); advance(&replay,steps);
        assert(memcmp(&state,&replay,sizeof(state))==0);
        FILE *checkpoint=tmpfile(); assert(checkpoint && growth_save(checkpoint,&state)); rewind(checkpoint);
        memset(&replay,0,sizeof(replay)); assert(growth_load(checkpoint,&replay)); fclose(checkpoint);
        assert(memcmp(&state,&replay,sizeof(state))==0);
        checkpoint=tmpfile(); assert(checkpoint); fputs("CRYSTAL_GROWTH_1\n",checkpoint); rewind(checkpoint);
        assert(!growth_load(checkpoint,&replay)); fclose(checkpoint); assert(memcmp(&state,&replay,sizeof(state))==0);
        assert(growth_validate(&state,&metrics));
        assert(metrics.vertices==98 && metrics.edges==288 && metrics.faces==192);
        assert(metrics.volume>seed.volume*2);
        printf("%d\t%llu\t%.9g\t%d\t%d\t%d\t%.12g\t%.12g\t%.12g\t%.12g\t%.12g\n",kind,(unsigned long long)state.steps,state.time,metrics.vertices,metrics.edges,metrics.faces,seed.volume,metrics.volume,metrics.area,metrics.minimum_radius,metrics.maximum_radius);
        state.paused=true; replay=state; assert(growth_step(&state,.02)); assert(memcmp(&state,&replay,sizeof(state))==0);
        state.paused=false; replay.paused=false; advance(&state,30); advance(&replay,30); assert(memcmp(&state,&replay,sizeof(state))==0);
        replay=state; assert(!growth_step(&state,NAN)); assert(!growth_step(&state,-1)); assert(!growth_step(&state,.251)); assert(memcmp(&state,&replay,sizeof(state))==0);
        assert(growth_step(&state,0)); assert(memcmp(&state,&replay,sizeof(state))==0);
        mutant=state; mutant.face[0].node[0]=GROWTH_NODES; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.face[0].node[0]=mutant.face[0].node[1]; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.face[0]=mutant.face[1]; assert(!growth_validate(&mutant,NULL));
        mutant=state; int swap=mutant.face[0].node[0]; mutant.face[0].node[0]=mutant.face[0].node[1]; mutant.face[0].node[1]=swap; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.position[0].x=NAN; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.position[0]=mutant.position[97]; mutant.direction[0]=mutant.direction[97]; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.surface_normal[0]=(GrowthVector){0}; assert(!growth_validate(&mutant,NULL));
        mutant=state; mutant.time=10000; replay=mutant; assert(!growth_step(&mutant,.02)); assert(memcmp(&mutant,&replay,sizeof(mutant))==0);
    }
    GrowthLaw law=growth_default_law(GROWTH_HALITE); law.redistribution=0;
    assert(growth_initialize(&state,&law,1));
    /* Independent cube expectation: axis h=.12+.24t and corner radius sqrt(3)h.
     * The seed is spherical, so this ratio must change with time. Scaling fails. */
    assert(fabs(growth_front_radius(&state,(GrowthVector){1,0,0},0)-.12)<1e-12);
    double axis=growth_front_radius(&state,(GrowthVector){1,0,0},4);
    double corner=growth_front_radius(&state,(GrowthVector){1,1,1},4);
    assert(fabs(axis-1.08)<1e-12 && fabs(corner/axis-sqrt(3))<1e-12);
    advance(&state,200);
    for(int n=0;n<GROWTH_NODES;n++) {
        GrowthVector p=state.position[n];
        double on_cube=fmax(fabs(p.x),fmax(fabs(p.y),fabs(p.z)));
        assert(fabs(on_cube-1.08)<1e-10);
    }
    GrowthMetrics cube_metrics; assert(growth_validate(&state,&cube_metrics));
    assert(cube_metrics.maximum_radius/cube_metrics.minimum_radius>1.65);
    /* Changing only relative family speed produces an octahedral competitor. */
    law.speed[1]=.15; assert(growth_initialize(&replay,&law,1));
    assert(growth_front_radius(&replay,(GrowthVector){1,1,1},4)<corner*.75);
    law=growth_default_law(GROWTH_HALITE); law.speed[2]=.1; assert(growth_initialize(&replay,&law,1));
    assert(growth_front_radius(&replay,(GrowthVector){1,1,0},4)<1);
    law=growth_default_law(GROWTH_PYRITE); assert(growth_initialize(&state,&law,1));
    int diploidal_difference=0;
    for(int i=1;i<100;i++) {
        GrowthVector d={i*.01,.31,.79},swapped={.31,i*.01,.79};
        if(fabs(growth_front_radius(&state,d,8)-growth_front_radius(&state,swapped,8))>1e-5) diploidal_difference++;
    }
    assert(diploidal_difference>10);
    double pyritohedral=growth_front_radius(&state,(GrowthVector){1,1,.2},8);
    law.speed[1]=.1; assert(growth_initialize(&replay,&law,1));
    assert(growth_front_radius(&replay,(GrowthVector){1,1,.2},8)<pyritohedral*.7);
    law=growth_default_law(GROWTH_PYRITE); law.speed[2]=.08; assert(growth_initialize(&replay,&law,1));
    assert(growth_front_radius(&replay,(GrowthVector){1,1,1},8)<1);
    law=growth_default_law(GROWTH_QUARTZ); assert(growth_initialize(&state,&law,1));
    law.handedness=-1; assert(growth_initialize(&replay,&law,1));
    int chiral_difference=0;
    for(int i=1;i<100;i++) {
        GrowthVector d={sin(i*.3),.7,cos(i*.3)},mirror={d.x,d.y,-d.z};
        double right=growth_front_radius(&state,d,5),left=growth_front_radius(&replay,mirror,5);
        assert(fabs(right-left)<1e-11);
        if(fabs(right-growth_front_radius(&replay,d,5))>1e-5) chiral_difference++;
    }
    assert(chiral_difference>10);
    law=growth_default_law(GROWTH_QUARTZ); assert(growth_initialize(&state,&law,1));
    law.frame[0]=0; law.frame[1]=-1; law.frame[3]=1; law.frame[4]=0;
    assert(growth_initialize(&replay,&law,1));
    assert(fabs(growth_front_radius(&state,(GrowthVector){0,1,0},5)-growth_front_radius(&replay,(GrowthVector){-1,0,0},5))<1e-11);
    law=growth_default_law(GROWTH_BISMUTH); law.hopper=false; assert(growth_initialize(&state,&law,1));
    law.hopper=true; assert(growth_initialize(&replay,&law,1));
    advance(&state,200); advance(&replay,200);
    double hopper_difference=0; for(int i=0;i<GROWTH_NODES;i++) hopper_difference+=distance(state.position[i],replay.position[i]);
    assert(hopper_difference>.1); assert(growth_validate(&replay,NULL));
    int retained_ledges=0,aligned_terraces=0;
    for(int n=0;n<GROWTH_NODES;n++) if(replay.terrace_credit[n]>1e-5 && replay.ledge_age[n]>0) {
        retained_ledges++;
        GrowthVector p=replay.position[n],normal=replay.surface_normal[n];
        double level=(p.x*normal.x+p.y*normal.y+p.z*normal.z-.12)/.08;
        if(fabs(level-round(level))<1e-8) aligned_terraces++;
    }
    assert(retained_ledges>0);
    assert(aligned_terraces>0);
    FILE *hopper_checkpoint=tmpfile(); assert(hopper_checkpoint && growth_save(hopper_checkpoint,&replay)); rewind(hopper_checkpoint);
    assert(growth_load(hopper_checkpoint,&state)); fclose(hopper_checkpoint); assert(memcmp(&state,&replay,sizeof(state))==0);
    advance(&state,50); advance(&replay,50); assert(memcmp(&state,&replay,sizeof(state))==0);
    law=growth_default_law(GROWTH_HALITE); law.frame[0]=2; assert(!growth_initialize(&state,&law,1));
    law=growth_default_law(GROWTH_HALITE); law.speed[0]=NAN; assert(!growth_initialize(&state,&law,1));
    assert(!growth_initialize(NULL,&law,1)); assert(!growth_validate(NULL,NULL));
    puts("analytic symmetry, cube dominance, family competition, chirality, hopper, determinism, pause, invalid-input mutations: PASS");
    return 0;
}

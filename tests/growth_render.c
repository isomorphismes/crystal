#include "../app/src/main/cpp/crystal_growth_surface.h"
#include "../app/src/main/cpp/crystal_render.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static CrystalGrowth state;
int main(void) {
    for(int kind=0;kind<4;kind++) {
        GrowthLaw law=growth_default_law((GrowthMaterial)kind); law.redistribution=0;
        assert(growth_initialize(&state,&law,1));
        for(int step=0;step<200;step++) assert(growth_step(&state,.02));
        CrystalSurface surface; assert(growth_surface(&state,&surface));
        CrystalTriangle triangles[MAX_TRIANGLES]; CrystalEdge edges[MAX_NET_EDGES]; int count=0;
        assert(triangulate_surface(&surface,triangles,&count) && count==192);
        assert(collect_net_edges(&surface,edges)==288);
        int boundaries=collect_boundary_edges(&surface,edges); assert(boundaries>=12 && boundaries<=288);
        if(kind==0) {
            /* A subdivided cube has four samples along each of its twelve
             * physical edges. The 240 interior subdivisions must not be drawn. */
            assert(boundaries==48);
            for(int e=0;e<boundaries;e++) {
                Vector3 a=surface.embedding.nodes[edges[e].a].position,b=surface.embedding.nodes[edges[e].b].position;
                int shared_boundary=(fabsf(fabsf(a.x)-1.08f)<1e-5f && fabsf(fabsf(b.x)-1.08f)<1e-5f)
                                  +(fabsf(fabsf(a.y)-1.08f)<1e-5f && fabsf(fabsf(b.y)-1.08f)<1e-5f)
                                  +(fabsf(fabsf(a.z)-1.08f)<1e-5f && fabsf(fabsf(b.z)-1.08f)<1e-5f);
                assert(shared_boundary==2);
            }
        }
        printf("material %d: %d sampled triangles, %d boundary segments\n",kind,count,boundaries);
        CrystalSurface invalid=surface; invalid.net.facets[50].count=999; assert(!validate_surface(&invalid));
    }
    return 0;
}

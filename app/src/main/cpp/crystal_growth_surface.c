#include "crystal_growth_surface.h"
bool growth_surface(const CrystalGrowth *growth,CrystalSurface *surface) {
    if(!surface || !growth_validate(growth,NULL)) return false;
    CrystalSurface candidate={0};
    candidate.net.node_count=GROWTH_NODES; candidate.net.facet_count=GROWTH_FACES;
    for(int n=0;n<GROWTH_NODES;n++) {
        GrowthVector point=growth->position[n];
        candidate.embedding.nodes[n].position=(Vector3){(float)point.x,(float)point.y,(float)point.z};
    }
    for(int f=0;f<GROWTH_FACES;f++) {
        candidate.net.facets[f].count=3;
        for(int c=0;c<3;c++) candidate.net.facets[f].node[c]=growth->face[f].node[c];
    }
    if(!validate_surface(&candidate)) return false;
    *surface=candidate; return true;
}

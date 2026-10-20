#include "crystal_model.h"
#include "raymath.h"
#include <math.h>
#include <string.h>
/* Validate the closed, oriented, piecewise-flat material surface before the
 * renderer can infer flat normals. Geometry tests explicitly mutate each rule.
 * This is a mesoscopic net, not an atomic lattice or growth simulation. */
bool validate_surface(const CrystalSurface *surface) {
    if (!surface || surface->net.node_count<=0 || surface->net.node_count>MAX_NET_NODES ||
        surface->net.facet_count<=0 || surface->net.facet_count>MAX_NET_FACETS) return false;
    for (int n=0;n<surface->net.node_count;n++) {
        Vector3 p=surface->embedding.nodes[n].position;
        if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z)) return false;
    }
    /* Validate every neighbor's metadata before the incidence walk can read it. */
    for(int f=0;f<surface->net.facet_count;f++) {
        const CrystalFacet *facet=&surface->net.facets[f];
        if(facet->count<3 || facet->count>MAX_FACET_NODES) return false;
        for(int c=0;c<facet->count;c++) if(facet->node[c]<0 || facet->node[c]>=surface->net.node_count) return false;
    }
    for (int f=0;f<surface->net.facet_count;f++) {
        const CrystalFacet *facet=&surface->net.facets[f];
        if (facet->count<3 || facet->count>MAX_FACET_NODES) return false;
        for (int v=0;v<facet->count;v++) {
            int id=facet->node[v];
            if (id<0 || id>=surface->net.node_count) return false;
            for (int w=v+1;w<facet->count;w++) if (id==facet->node[w]) return false;
        }
        Vector3 p0=surface->embedding.nodes[facet->node[0]].position;
        Vector3 p1=surface->embedding.nodes[facet->node[1]].position;
        Vector3 p2=surface->embedding.nodes[facet->node[2]].position;
        Vector3 normal=Vector3CrossProduct(Vector3Subtract(p1,p0),Vector3Subtract(p2,p0));
        float squared=Vector3DotProduct(normal,normal);
        if (!(squared>1e-10f) || !isfinite(squared)) return false;
        if (facet->count==4) {
            Vector3 p3=surface->embedding.nodes[facet->node[3]].position;
            Vector3 other=Vector3CrossProduct(Vector3Subtract(p2,p0),Vector3Subtract(p3,p0));
            if (!(Vector3DotProduct(normal,other)>0.0f)) return false;
            float offset=fabsf(Vector3DotProduct(normal,Vector3Subtract(p3,p0)))/sqrtf(squared);
            float scale=1.0f+fmaxf(Vector3Distance(p1,p0),
                                   fmaxf(Vector3Distance(p2,p0),Vector3Distance(p3,p0)));
            if (!(offset<=1e-5f*scale)) return false;
        }
        /* Every directed boundary edge must have exactly one reverse on the
         * adjacent facet. A flipped face or open seam cannot be silently lit. */
        for (int v=0;v<facet->count;v++) {
            int a=facet->node[v],b=facet->node[(v+1)%facet->count];
            int forwards=0,backwards=0;
            for (int g=0;g<surface->net.facet_count;g++) {
                const CrystalFacet *neighbor=&surface->net.facets[g];
                for (int k=0;k<neighbor->count;k++) {
                    int x=neighbor->node[k],y=neighbor->node[(k+1)%neighbor->count];
                    if (x==a && y==b) forwards++;
                    if (x==b && y==a) backwards++;
                }
            }
            if (forwards!=1 || backwards!=1) return false;
        }
    }
    return true;
}

static int add_surface_node(CrystalSurface *net, float x, float y, float z) {
    if (net->net.node_count >= MAX_NET_NODES) return -1;
    int id = net->net.node_count++;
    net->embedding.nodes[id].position = (Vector3){ x, y, z };
    return id;
}

static bool add_triangle_facet(CrystalSurface *net, int a, int b, int c) {
    if (net->net.facet_count >= MAX_NET_FACETS) return false;
    CrystalFacet *f = &net->net.facets[net->net.facet_count++];
    f->count = 3;
    f->node[0] = a; f->node[1] = b; f->node[2] = c;
    return true;
}

static bool add_quadrilateral_facet(CrystalSurface *net, int a, int b, int c, int d) {
    if (net->net.facet_count >= MAX_NET_FACETS) return false;
    CrystalFacet *f = &net->net.facets[net->net.facet_count++];
    f->count = 4;
    f->node[0] = a; f->node[1] = b; f->node[2] = c; f->node[3] = d;
    return true;
}

CrystalSurface build_halite_surface(void) {
    CrystalSurface net = {0};
    const float s = 1.35f;
    int n[8];
    n[0] = add_surface_node(&net, -s, -s, -s);
    n[1] = add_surface_node(&net,  s, -s, -s);
    n[2] = add_surface_node(&net,  s,  s, -s);
    n[3] = add_surface_node(&net, -s,  s, -s);
    n[4] = add_surface_node(&net, -s, -s,  s);
    n[5] = add_surface_node(&net,  s, -s,  s);
    n[6] = add_surface_node(&net,  s,  s,  s);
    n[7] = add_surface_node(&net, -s,  s,  s);

    add_quadrilateral_facet(&net, n[0], n[1], n[2], n[3]);
    add_quadrilateral_facet(&net, n[4], n[7], n[6], n[5]);
    add_quadrilateral_facet(&net, n[0], n[4], n[5], n[1]);
    add_quadrilateral_facet(&net, n[1], n[5], n[6], n[2]);
    add_quadrilateral_facet(&net, n[2], n[6], n[7], n[3]);
    add_quadrilateral_facet(&net, n[3], n[7], n[4], n[0]);
    return net;
}

CrystalSurface build_quartz_surface(void) {
    CrystalSurface net = {0};
    int upper[6], lower[6];
    const float radius = 1.25f;
    const float yUpper = 0.85f;
    const float yLower = -0.85f;

    for (int i = 0; i < 6; ++i) {
        float angle = (float)i * (2.0f*PI/6.0f) + PI/6.0f;
        float x = radius*cosf(angle);
        float z = radius*sinf(angle);
        upper[i] = add_surface_node(&net, x, yUpper, z);
        lower[i] = add_surface_node(&net, x, yLower, z);
    }

    int top = add_surface_node(&net, 0.0f, 1.85f, 0.0f);
    int bottom = add_surface_node(&net, 0.0f, -1.85f, 0.0f);

    for (int i = 0; i < 6; ++i) {
        int j = (i + 1) % 6;
        add_quadrilateral_facet(&net, lower[i], lower[j], upper[j], upper[i]);
        add_triangle_facet(&net, upper[i], upper[j], top);
        add_triangle_facet(&net, lower[j], lower[i], bottom);
    }
    return net;
}

static void add_square_ring(CrystalSurface *net, float half, float y, int out[4]) {
    out[0] = add_surface_node(net, -half, y, -half);
    out[1] = add_surface_node(net,  half, y, -half);
    out[2] = add_surface_node(net,  half, y,  half);
    out[3] = add_surface_node(net, -half, y,  half);
}

static void add_ring_band(CrystalSurface *net, const int outer[4], const int inner[4]) {
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        add_quadrilateral_facet(net, outer[i], outer[j], inner[j], inner[i]);
    }
}

CrystalSurface build_bismuth_surface(void) {
    CrystalSurface net = {0};
    const int levels = 6;
    const float startHalf = 1.70f;
    const float step = 0.22f;
    const float drop = 0.22f;

    int outer[4];
    add_square_ring(&net, startHalf, 0.78f, outer);

    int outsideBottom[4];
    add_square_ring(&net, startHalf, -0.95f, outsideBottom);
    for (int i = 0; i < 4; ++i) {
        int j = (i + 1) % 4;
        add_quadrilateral_facet(&net, outsideBottom[i], outsideBottom[j], outer[j], outer[i]);
    }
    add_quadrilateral_facet(&net, outsideBottom[3], outsideBottom[2], outsideBottom[1], outsideBottom[0]);

    float currentY = 0.78f;
    float currentHalf = startHalf;

    for (int level = 0; level < levels; ++level) {
        float innerHalf = currentHalf - step;
        int innerTop[4];
        add_square_ring(&net, innerHalf, currentY, innerTop);
        add_ring_band(&net, outer, innerTop);

        float nextY = currentY - drop;
        int innerBottom[4];
        add_square_ring(&net, innerHalf, nextY, innerBottom);
        for (int i = 0; i < 4; ++i) {
            int j = (i + 1) % 4;
            add_quadrilateral_facet(&net, innerTop[i], innerTop[j], innerBottom[j], innerBottom[i]);
        }

        memcpy(outer, innerBottom, sizeof(outer));
        currentHalf = innerHalf;
        currentY = nextY;
    }

    add_quadrilateral_facet(&net, outer[0], outer[1], outer[2], outer[3]);
    return net;
}

bool triangulate_surface(const CrystalSurface *net, CrystalTriangle out[MAX_TRIANGLES], int *triangle_count) {
    if (!validate_surface(net)) return false;
    int count = 0;
    for (int f = 0; f < net->net.facet_count; ++f) {
        const CrystalFacet *facet = &net->net.facets[f];
        if (facet->count < 3 || facet->count > MAX_FACET_NODES) return false;
        for (int i = 0; i < facet->count; ++i) {
            if (facet->node[i] < 0 || facet->node[i] >= net->net.node_count) return false;
        }
        for (int i = 1; i + 1 < facet->count; ++i) {
            if (count >= MAX_TRIANGLES) return false;
            out[count++] = (CrystalTriangle){
                net->embedding.nodes[facet->node[0]].position,
                net->embedding.nodes[facet->node[i]].position,
                net->embedding.nodes[facet->node[i + 1]].position
            };
        }
    }
    *triangle_count = count;
    return true;
}

int collect_net_edges(const CrystalSurface *net, CrystalEdge out[MAX_NET_EDGES]) {
    if (!validate_surface(net)) return -1;
    int edge_count = 0;

    for (int f = 0; f < net->net.facet_count; ++f) {
        const CrystalFacet *facet = &net->net.facets[f];
        for (int i = 0; i < facet->count; ++i) {
            int a = facet->node[i];
            int b = facet->node[(i + 1) % facet->count];
            if (a > b) {
                int tmp = a;
                a = b;
                b = tmp;
            }

            bool duplicate = false;
            for (int e = 0; e < edge_count; ++e) {
                if (out[e].a == a && out[e].b == b) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                if (edge_count >= MAX_NET_EDGES) return -1;
                out[edge_count++] = (CrystalEdge){ a, b };
            }
        }
    }

    return edge_count;
}


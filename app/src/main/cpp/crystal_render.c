#include "crystal_render.h"
#include <stddef.h>
#include "raymath.h"
Mesh build_raylib_mesh(const CrystalSurface *net) {
    CrystalTriangle triangles[MAX_TRIANGLES];
    int triangle_count = 0;
    Mesh mesh = {0};

    if (!triangulate_surface(net, triangles, &triangle_count) || triangle_count <= 0) {
        TraceLog(LOG_ERROR, "Crystal: net triangulation failed");
        return mesh;
    }

    mesh.triangleCount = triangle_count;
    mesh.vertexCount = triangle_count * 3;
    mesh.vertices = (float *)MemAlloc((size_t)mesh.vertexCount * 3u * sizeof(float));
    mesh.normals = (float *)MemAlloc((size_t)mesh.vertexCount * 3u * sizeof(float));

    for (int t = 0; t < triangle_count; ++t) {
        Vector3 p[3] = { triangles[t].a, triangles[t].b, triangles[t].c };
        Vector3 ab = Vector3Subtract(p[1], p[0]);
        Vector3 ac = Vector3Subtract(p[2], p[0]);
        Vector3 normal = Vector3Normalize(Vector3CrossProduct(ab, ac));

        for (int v = 0; v < 3; ++v) {
            int base = (t*3 + v)*3;
            mesh.vertices[base + 0] = p[v].x;
            mesh.vertices[base + 1] = p[v].y;
            mesh.vertices[base + 2] = p[v].z;
            mesh.normals[base + 0] = normal.x;
            mesh.normals[base + 1] = normal.y;
            mesh.normals[base + 2] = normal.z;
        }
    }

    UploadMesh(&mesh, true);
    return mesh;
}

bool update_raylib_mesh(Mesh *mesh,const CrystalSurface *surface) {
    CrystalTriangle triangles[MAX_TRIANGLES]; int count=0;
    if(!mesh || !triangulate_surface(surface,triangles,&count) || count*3!=mesh->vertexCount) return false;
    for(int t=0;t<count;t++) {
        Vector3 points[3]={triangles[t].a,triangles[t].b,triangles[t].c};
        Vector3 normal=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(points[1],points[0]),Vector3Subtract(points[2],points[0])));
        for(int corner=0;corner<3;corner++) {
            int offset=(t*3+corner)*3;
            mesh->vertices[offset]=points[corner].x; mesh->vertices[offset+1]=points[corner].y; mesh->vertices[offset+2]=points[corner].z;
            mesh->normals[offset]=normal.x; mesh->normals[offset+1]=normal.y; mesh->normals[offset+2]=normal.z;
        }
    }
    UpdateMeshBuffer(*mesh,0,mesh->vertices,mesh->vertexCount*3*(int)sizeof(float),0);
    UpdateMeshBuffer(*mesh,2,mesh->normals,mesh->vertexCount*3*(int)sizeof(float),0);
    return true;
}
int collect_boundary_edges(const CrystalSurface *surface,CrystalEdge out[MAX_NET_EDGES]) {
    CrystalEdge edges[MAX_NET_EDGES]; int count=collect_net_edges(surface,edges),visible=0;
    if(count<0) return -1;
    for(int edge=0;edge<count;edge++) {
        Vector3 normals[2]; int incident=0;
        for(int f=0;f<surface->net.facet_count;f++) {
            const CrystalFacet *facet=&surface->net.facets[f];
            bool first=false,second=false;
            for(int c=0;c<facet->count;c++) { if(facet->node[c]==edges[edge].a) first=true; if(facet->node[c]==edges[edge].b) second=true; }
            if(first && second) {
                if(incident>=2) return -1;
                Vector3 a=surface->embedding.nodes[facet->node[0]].position,b=surface->embedding.nodes[facet->node[1]].position,c=surface->embedding.nodes[facet->node[2]].position;
                normals[incident++]=Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b,a),Vector3Subtract(c,a)));
            }
        }
        if(incident!=2) return -1;
        /* An edge between coplanar samples is an internal subdivision, not a
         * visible crystal boundary. Keep the incidence in the simulation. */
        if(Vector3DotProduct(normals[0],normals[1])<1.0f-1e-5f) out[visible++]=edges[edge];
    }
    return visible;
}

void draw_net_edges(const CrystalSurface *net, const CrystalEdge *edges, int edge_count,
                         Matrix transform, Color color) {
    for (int e = 0; e < edge_count; ++e) {
        Vector3 a = Vector3Transform(net->embedding.nodes[edges[e].a].position, transform);
        Vector3 b = Vector3Transform(net->embedding.nodes[edges[e].b].position, transform);
        DrawLine3D(a, b, color);
    }
}


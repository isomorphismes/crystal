#ifndef CRYSTAL_RENDER_H
#define CRYSTAL_RENDER_H
#include "crystal_model.h"
Mesh build_raylib_mesh(const CrystalSurface *surface);
bool update_raylib_mesh(Mesh *mesh,const CrystalSurface *surface);
int collect_boundary_edges(const CrystalSurface *surface,CrystalEdge out[MAX_NET_EDGES]);
void draw_net_edges(const CrystalSurface *surface,const CrystalEdge *edges,int count,Matrix transform,Color color);
#endif

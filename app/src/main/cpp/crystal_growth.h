#ifndef CRYSTAL_GROWTH_H
#define CRYSTAL_GROWTH_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define GROWTH_NODES 98
#define GROWTH_FACES 192
#define GROWTH_PLANES 64
typedef struct { double x,y,z; } GrowthVector;
typedef struct { int node[3]; } GrowthFace;
typedef enum { GROWTH_HALITE, GROWTH_QUARTZ, GROWTH_BISMUTH, GROWTH_PYRITE } GrowthMaterial;
typedef struct {
    GrowthMaterial material;
    double speed[3], baseline, seed_radius, redistribution;
    double frame[9]; /* material to world, row-major proper orthogonal matrix */
    int handedness;
    bool hopper;
} GrowthLaw;
typedef struct { GrowthVector normal; double speed; int family; } GrowthPlane;
typedef struct {
    GrowthLaw law;
    GrowthPlane planes[GROWTH_PLANES];
    int plane_count;
    GrowthVector direction[GROWTH_NODES], position[GROWTH_NODES];
    GrowthVector surface_normal[GROWTH_NODES];
    double normal_speed[GROWTH_NODES];
    double terrace_credit[GROWTH_NODES];
    unsigned int ledge_age[GROWTH_NODES];
    GrowthFace face[GROWTH_FACES];
    double time;
    uint64_t steps, seed;
    bool paused;
} CrystalGrowth;
typedef struct {
    double volume, area, minimum_radius, maximum_radius, minimum_solid_angle;
    int vertices, edges, faces;
} GrowthMetrics;
GrowthLaw growth_default_law(GrowthMaterial material);
bool growth_initialize(CrystalGrowth *state, const GrowthLaw *law, uint64_t seed);
bool growth_step(CrystalGrowth *state, double delta_time);
bool growth_validate(const CrystalGrowth *state, GrowthMetrics *metrics);
bool growth_save(FILE *file,const CrystalGrowth *state);
bool growth_load(FILE *file,CrystalGrowth *state);
/* Exact radial intersection with the configured moving material planes. */
double growth_front_radius(const CrystalGrowth *state, GrowthVector direction, double time);
#endif

# Mostow → Crystal: facet lighting and shape checks

Origin: [Flexible Pipes #87](https://github.com/isomorphisms/flexible-pipes/issues/87).
Reference: `isomorphismes/mostow@a33da13baac446c7b6a22862f41aed444129b3ca`,
`android/mostow_renderer.c`.

Crystal keeps its C/Lua/raylib application and its **flat per-triangle normals**,
generated in `crystal_render.c`. Unlike Flower and Mostow, averaging normals
between facets would erase the actual geometric facet boundaries. The new shader
uses the raylib `matNormal` normal matrix, so its lighting remains correct when
the existing model rotates; the Lua-configured RGBA color is supplied exactly once
as a shader uniform, including its alpha channel. The earlier separate edge-overlay, x-ray, zoom, freeze,
and camera diagnostics remain intact.

The CPU validation now rejects repeated facet corners, degenerate triangles,
nonplanar quadrilaterals, reversed local quadrilateral triangles, open edges
and inconsistent adjacent facet winding. Current Halite, Quartz and Bismuth
surfaces are still the intended acceptance cases. It does not claim to verify
outward vs inward orientation, physical crystallographic truth, or a growth law.
No hyperbolic metric or Mostow animation is imported.

The GLSL 100 / 330 shader is checked for required raylib semantic locations at
application startup. Existing host geometry tests check normal preconditions
and negative mutations. These are not actual GPU shader compile/render tests.

**NOT_VERIFIED:** physical MIRO A1/C67 GLES2 lighting, thermal/performance,
Android build/signing producer, test APK deployment, and ICK/Bionic application
qualification. The parked Crystal #7 integration line and its separate producer
blockers are not marked complete.

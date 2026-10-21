# Executable crystal growth candidate

This replaces the application’s static material generators with one native C
front and one 98-node, 192-triangle sampling complex. The old template builders
remain regression fixtures. They are not called by the application growth path.

The state separates material law/frame, evolving time, sample directions,
embedded positions, incidence, and hopper/terrace memory. The renderer receives a
validated float conversion and never owns simulation state. Identical law, seed
identity and step sequence reproduce identical state. The seed identity is
retained for replay; this first seed is spherical and does not use randomness.

## Constitutive model

The provisional speeds in the existing material TOML files supply the three
family speeds in `growth_default_law`. These are configuration values, not
experimental kinetics or an atomic lattice. The C configuration is currently
the executable input; a TOML loader is not claimed.

Each material defines a symmetry orbit of unit plane normals in its own frame.
At time t each plane’s offset is seed_radius + speed × t. A baseline spherical
front bounds directions not yet captured by a material plane. The radius along
a world direction is the minimum positive intersection with those moving
planes and the baseline sphere. At time zero every node samples the same small
sphere. Slower planes progressively replace that sphere, so shape ratios change
with time. This is a finite-support crystalline-front collocation model, not a
triangular-mesh discretization of a general anisotropic curvature PDE.

The active supporting plane supplies the analytic surface normal and normal
speed; its normal is transformed from the material frame to world coordinates.
Ray directions transform in the opposite direction before intersection. Mesh
triangle normals are independently computed and validated for rendering. They
approximate the analytic front between samples and are not averaged across
rendered triangles.

Halite has full signed cubic {100}, {111} and {110} orbits. Pyrite uses the 12
signed cyclic {210} directions, not all 24 signed permutations, alongside {100}
and {111}. Quartz uses proper D3 orbits, competing prism/rhombohedral speeds and
a signed phase; opposite phases are mirror related. Bismuth uses centrosymmetric
trigonal orbits as a coarse A7-inspired anisotropy. These chosen normal slopes
are mesoscopic approximations, not calibrated Miller-to-Cartesian kinetics.

Bismuth’s configurable hopper mode slows face centers relative to edges.
Unreleased advance is stored per node until a material-normal terrace level
with spacing 0.08 is reached, or 48 steps of ledge retention expire. This is
retained simulation state, including in checkpoints. Hopper-off uses the same
engine without that kinetic rule. Physical appearance and usefulness of this
coarse terrace sampling remain unverified.

## Fixed-budget remeshing and rejection

Every step advances time by a bounded finite increment, redistributes directions
in the tangent plane, resamples the moving front, and validates transactionally.
Tangential Laplacian relaxation regulates local chord lengths and angles;
nearby competing-plane bisectors attract samples toward facet boundaries. Once
per 64 steps, a legal diagonal flip may improve the worst local spherical
triangle quality. No operation adds or removes nodes.

The validator checks finite coordinates, positive radii, normalized normals,
positive oriented triangles, exactly two opposite incidences per edge, one
cyclic link per vertex, Euler characteristic 2 and total spherical area 4π.
For this radial genus-zero representation, positive faces and total degree one
exclude overlapping triangle interiors: a regular ray has one positive
preimage. This proof relies on the radial representation and must not be
reused as a general nonradial triangle-intersection algorithm. Invalid proposed
remeshing is discarded while the front can advance on its preceding valid net;
invalid front evolution is rejected without modifying the caller’s state.

## Application and package boundary

The application updates dynamic vertex/normal buffers, displays growth time,
step count and volume, and provides pause, seed restart, orbit, zoom, fit and
occluded/x-ray edge modes. Coplanar internal subdivisions are excluded from
boundary drawing. User zoom disables automatic fit until FIT is selected.
Far clipping expands with the model. The existing GLES 100/desktop GLSL 330
lighting candidate is retained from the facet-lighting branch as source work;
it has not acquired physical GPU acceptance.

Checkpoint encoding round-trips simulation state, including remeshing and ledge
memory. Android pause/save/focus callbacks write an atomic checkpoint in the
application’s observed private directory and preserve raylib’s prior callback.
No lifecycle callback or displayed frame has been exercised on a physical C67.

The native recipe now declares ARM64 and all four material variants. The package
recipe derives ABI from its native receipt, uses the canonical Android-NDK
packager and produces the requested individual C67 filenames. Existing package
IDs and signer selection rules remain intact. All four ARM64 native variants
have now linked at commit `e745edd98f5b8c854fbba4c72bb0a0a699aa50b5`.
These are native candidates; no signed APK has been produced.

## Executed qualification and remaining obligations

Pinned NDK r27c compiler and linker hashes were checked before host execution.
`tests/run-host.ysh` ran through Grease at the exact current source gitlink
5651cf97a1b5042f24f14112a7ade9a1518eb0bc and a local Flexible Pipes envelope.
The inherited Flexible Pipes Python runner remains migration debt; no new
Python program was added. Early system-compiler exploratory runs are excluded
from maintained qualification. Host results and actual shape metrics are under
`evidence/growth-c67/`.

The suite executes 10,000 steps per material, replay, pause/checkpoint round
trips, invalid-input and incidence mutations, symmetry, orientation-frame
covariance, quartz mirror equivalence, family-weight competition, Halite cube
dominance and retained bismuth terrace state. Three compiled implementation
mutants erase speeds, handedness or diploidal distinction; each fails its named
independent expectation. The render adapter’s cube has 48 segments along its
12 boundaries, rather than 288 visible sampling edges. The host suite passed
again on October 21. The growth and rendering-adapter translation units are
unchanged from that run; application diagnostics subsequently received a larger
snapshot buffer and an additional ARM64 format check.

`evidence/growth-c67/native-20261021/` records a clean, exact-commit ARM64
compile/link/strip of all four variants through Flexible Pipes. Each stripped
library is 1,178,352 bytes. The retained AI-CI verifier source at
`5c80279dfcee565f4e999d2b1f97feb966b8efd3` reproduced its required executable
hash before the unchanged native recipe ran. Its own contract/signing cases
passed; this reproduces a candidate verifier and does not independently accept
Crystal's mathematics or authorize the shared producer.

Consumer ELF inspection checks the recorded library hashes, ELF64/AArch64,
exported NativeActivity entry point, non-executable stack, Android platform-only
dependencies, and absence of TEXTREL/RPATH/RUNPATH. This does not exercise GLES
or the Android lifecycle. The native build retains an upstream Lua `tmpnam`
deprecation warning. Native hashes and source hashes are in the evidence
directory; they are not APK hashes.

Wireframe-only occlusion now submits a surface depth pass with color writes
disabled. Previously that mode had no surface depth and could show rear edges
despite selecting occlusion. Device diagnostics now report actual growth time,
step count, volume and pause status, and report rejected geometry as failure.
Neither this depth-pass repair nor shader appearance has displayed-device
acceptance.

Independent AI-CI mathematical/producer acceptance is PENDING. The approved FP
Android registry currently lists only Wegert, and its producer requires an ICK
object/paired recipe that Crystal does not have. No dummy object, new operation
authority or policy exception was supplied. Current AI-CI main lists the three
existing Crystal test identities; Pyrite has no entry. Installed C67 package
and certificate compatibility has not been observed. The earlier original-key
recovery obligation remains unresolved. No APK was signed, delivered, installed
or physically accepted, and no PR is eligible for merge on this host evidence.
The exact-commit GitHub run on October 21 failed before compilation with
`BLOCKED: registered Android operation is not deployed`:
https://github.com/isomorphismes/crystal/actions/runs/39597755932/job/119214209536.
The retained registered-operation work also requires qualified predecessor
decisions; the local native pipeline grants none of that authority. The
maintained Kitchen/producer route, approved signer provision and installed C67
update identity remain completion obligations.

Owners remain Crystal issues 2, 8 and 9. External sources in the separate
geometry-source PR were reviewed as references; none was imported into this
engine or relabeled as licensed runtime code.

## Findings for Flower

Fixed node identity, incidence and embedding can remain separate. An edge flip
preserves node count and sphere topology but changes adjacency and therefore
hop neighborhoods. Flower must decide separately whether that change is legal
for intrinsic/material neighborhoods; stable node IDs alone do not preserve
hop distance. Radial positivity is Crystal-specific and cannot validate
Flower’s open, ruffled or overhanging sheets. A useful common regression changes
embedding with incidence fixed, then changes a legal diagonal with geometry
fixed, checking intrinsic adjacency separately. Flower issue 2 owns that
downstream assessment; no Flower refactor follows merely from this candidate.

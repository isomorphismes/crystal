#ifndef CRYSTAL_SURFACE_SHADER_H
#define CRYSTAL_SURFACE_SHADER_H
/* Mostow-inspired lighting; Crystal remains a faceted material, never a
 * smoothed hyperbolic sheet. Raylib supplies mvp and inverse-transpose model
 * normal matrix even as the model rotates under its existing controls. */
#if defined(__ANDROID__) || defined(PLATFORM_ANDROID)
#define CRYSTAL_V_VERSION "#version 100\n"
#define CRYSTAL_V_IN "attribute"
#define CRYSTAL_V_OUT "varying"
#define CRYSTAL_F_VERSION "#version 100\nprecision mediump float;\n"
#define CRYSTAL_F_IN "varying"
#define CRYSTAL_F_DECL ""
#define CRYSTAL_F_RESULT "gl_FragColor"
#else
#define CRYSTAL_V_VERSION "#version 330\n"
#define CRYSTAL_V_IN "in"
#define CRYSTAL_V_OUT "out"
#define CRYSTAL_F_VERSION "#version 330\n"
#define CRYSTAL_F_IN "in"
#define CRYSTAL_F_DECL "out vec4 finalColor;\n"
#define CRYSTAL_F_RESULT "finalColor"
#endif

static const char crystal_facet_vertex_shader[] =
    CRYSTAL_V_VERSION
    CRYSTAL_V_IN " vec3 vertexPosition;\n"
    CRYSTAL_V_IN " vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    CRYSTAL_V_OUT " vec3 v_world_normal;\n"
    "void main(){\n"
    "  gl_Position=mvp*vec4(vertexPosition,1.0);\n"
    "  v_world_normal=(matNormal*vec4(vertexNormal,0.0)).xyz;\n"
    "}\n";

static const char crystal_facet_fragment_shader[] =
    CRYSTAL_F_VERSION
    CRYSTAL_F_IN " vec3 v_world_normal;\n"
    "uniform vec3 u_material_color;\n"
    CRYSTAL_F_DECL
    "void main(){\n"
    "  vec3 n=normalize(v_world_normal);\n"
    "  if (!gl_FrontFacing) n=-n;\n"
    "  vec3 key=normalize(vec3(-0.4,0.6,0.9));\n"
    "  vec3 fill=normalize(vec3(0.6,0.25,-0.7));\n"
    "  float diffuse=max(0.0,dot(n,key));\n"
    "  float side=max(0.0,dot(n,fill));\n"
    "  vec3 color=u_material_color*(0.33+0.52*diffuse+0.18*side);\n"
    "  " CRYSTAL_F_RESULT "=vec4(color,1.0);\n"
    "}\n";
#endif

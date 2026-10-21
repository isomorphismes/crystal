#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "diagnostic_state.h"
#include "crystal_model.h"
#include "crystal_render.h"
#include "crystal_growth.h"
#include "crystal_growth_surface.h"
#include "crystal_surface_shader.h"
#if defined(__ANDROID__)
#include <unistd.h>
#include <GLES2/gl2.h>
#include <android/log.h>
#include <android_native_app_glue.h>
extern struct android_app *GetAndroidApp(void);
extern FILE *__real_fopen(const char *path,const char *mode);
#endif

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#ifndef CRYSTAL_KIND
#define CRYSTAL_KIND 0
#endif

#ifndef CRYSTAL_TITLE
#define CRYSTAL_TITLE "Crystal"
#endif

#ifndef CRYSTAL_EMULATOR_TEST
#define CRYSTAL_EMULATOR_TEST 0
#endif

#define MAX_NET_NODES 160
#define MAX_NET_FACETS 192
#define MAX_FACET_NODES 4
#define MAX_TRIANGLES 512
#define MAX_NET_EDGES (MAX_NET_FACETS * MAX_FACET_NODES)
#ifndef CRYSTAL_SOURCE_COMMIT
#define CRYSTAL_SOURCE_COMMIT "UNKNOWN"
#endif
#ifndef CRYSTAL_PACKAGE_ID
#define CRYSTAL_PACKAGE_ID "UNKNOWN"
#endif

static Color tint = { 220, 232, 245, 255 };
static float auto_spin = 0.0f;
#if defined(__ANDROID__)
static CrystalGrowth *persistent_growth;
static bool *persistent_user_pause;
static void (*previous_android_command)(struct android_app *,int32_t);
static bool checkpoint_growth(struct android_app *app) {
    if(!persistent_growth || !app || !app->activity || !app->activity->internalDataPath) return false;
    char temporary[4096],destination[4096];
    int first=snprintf(temporary,sizeof(temporary),"%s/crystal-growth.state.tmp",app->activity->internalDataPath);
    int second=snprintf(destination,sizeof(destination),"%s/crystal-growth.state",app->activity->internalDataPath);
    if(first<0 || second<0 || (size_t)first>=sizeof(temporary) || (size_t)second>=sizeof(destination)) return false;
    CrystalGrowth checkpoint=*persistent_growth;
    if(persistent_user_pause) checkpoint.paused=*persistent_user_pause;
    FILE *file=__real_fopen(temporary,"w");
    if(!file) return false;
    bool saved=growth_save(file,&checkpoint);
    if(fflush(file)!=0 || fsync(fileno(file))!=0) saved=false;
    if(fclose(file)!=0) saved=false;
    if(saved && rename(temporary,destination)==0) return true;
    remove(temporary); return false;
}
static void crystal_android_command(struct android_app *app,int32_t command) {
    if(command==APP_CMD_SAVE_STATE || command==APP_CMD_PAUSE || command==APP_CMD_LOST_FOCUS || command==APP_CMD_STOP)
        if(!checkpoint_growth(app)) __android_log_write(ANDROID_LOG_ERROR,"CrystalState","simulation checkpoint failed; previous checkpoint retained");
    if(previous_android_command) previous_android_command(app,command);
}
static void restore_growth(CrystalGrowth *growth) {
    struct android_app *app=GetAndroidApp();
    char path[4096];
    int size=snprintf(path,sizeof(path),"%s/crystal-growth.state",app->activity->internalDataPath);
    if(size<0 || (size_t)size>=sizeof(path)) return;
    FILE *file=__real_fopen(path,"r");
    if(!file) return;
    CrystalGrowth restored;
    bool loaded=growth_load(file,&restored); fclose(file);
    if(loaded && restored.law.material==(GrowthMaterial)CRYSTAL_KIND) *growth=restored;
    else __android_log_write(ANDROID_LOG_ERROR,"CrystalState","checkpoint rejected; seed retained");
}
#endif

static int lua_set_color(lua_State *L) {
    double r = luaL_checknumber(L, 1);
    double g = luaL_checknumber(L, 2);
    double b = luaL_checknumber(L, 3);
    double a = luaL_optnumber(L, 4, 1.0);
    if (r < 0) r = 0; if (r > 1) r = 1;
    if (g < 0) g = 0; if (g > 1) g = 1;
    if (b < 0) b = 0; if (b > 1) b = 1;
    if (a < 0) a = 0; if (a > 1) a = 1;
    tint = (Color){
        (unsigned char)(r*255.0),
        (unsigned char)(g*255.0),
        (unsigned char)(b*255.0),
        (unsigned char)(a*255.0)
    };
    return 0;
}

static void json_safe(const char *input, char *output, size_t capacity) {
    size_t used=0;
    for (size_t i=0; input[i] && used+2<capacity; i++) {
        unsigned char c=(unsigned char)input[i];
        if (c=='"' || c=='\\') output[used++]='\\';
        output[used++]=(c<32)?'?':(char)c;
    }
    output[used]=0;
}

static void process_session(char *output, size_t capacity) {
    snprintf(output, capacity, "UNKNOWN");
#if defined(__ANDROID__)
    FILE *file=fopen("/proc/self/stat", "r");
    char stat[2048];
    if (file && fgets(stat, sizeof(stat), file)) {
        char *tail=strrchr(stat, ')');
        if (tail) {
            char *token=strtok(tail+1, " ");
            for (int field=3; token && field<22; field++) token=strtok(NULL, " ");
            if (token) snprintf(output, capacity, "%ld-%s", (long)getpid(), token);
        }
    }
    if (file) fclose(file);
#endif
}

static int lua_set_spin(lua_State *L) {
    auto_spin = (float)luaL_checknumber(L, 1);
    return 0;
}

static void configure_with_lua(void) {
    lua_State *L = luaL_newstate();
    if (!L) return;
    luaL_openlibs(L);

    lua_newtable(L);
    lua_pushcfunction(L, lua_set_color);
    lua_setfield(L, -2, "set_color");
    lua_pushcfunction(L, lua_set_spin);
    lua_setfield(L, -2, "set_auto_spin");
    lua_setglobal(L, "crystal");

#if CRYSTAL_KIND == 0
    const char *script =
        "crystal.set_color(0.72, 0.88, 1.00, 1.0)\n"
        "crystal.set_auto_spin(0.10)\n";
#elif CRYSTAL_KIND == 1
    const char *script =
        "crystal.set_color(0.82, 0.72, 0.96, 1.0)\n"
        "crystal.set_auto_spin(0.08)\n";
#else
    const char *script =
        "crystal.set_color(0.88, 0.55, 0.30, 1.0)\n"
        "crystal.set_auto_spin(0.06)\n";
#endif

    if (luaL_dostring(L, script) != LUA_OK) {
        TraceLog(LOG_WARNING, "Crystal Lua config failed: %s", lua_tostring(L, -1));
    }
    lua_close(L);
}

static float fit_distance(float radius,int width,int height,float field_of_view) {
    float vertical=field_of_view*DEG2RAD/2;
    float horizontal=atanf(tanf(vertical)*(float)width/(float)(height>0?height:1));
    return 1.15f*radius/sinf(fminf(vertical,horizontal));
}

#if !defined(CRYSTAL_HOST_TEST)
int main(void) {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(576, 1152, CRYSTAL_TITLE);
    SetTargetFPS(60);

    configure_with_lua();
    char launchSession[96];
    process_session(launchSession, sizeof(launchSession));
    rlSetClipPlanes(RL_CULL_DISTANCE_NEAR, RL_CULL_DISTANCE_FAR);
    const char *glVendor="UNKNOWN", *glRenderer="UNKNOWN", *glVersion="UNKNOWN";
#if defined(__ANDROID__)
    glVendor=(const char *)glGetString(GL_VENDOR);
    glRenderer=(const char *)glGetString(GL_RENDERER);
    glVersion=(const char *)glGetString(GL_VERSION);
    if (!glVendor) glVendor="UNKNOWN";
    if (!glRenderer) glRenderer="UNKNOWN";
    if (!glVersion) glVersion="UNKNOWN";
#endif

    char safeVendor[256], safeRenderer[256], safeVersion[256];
    json_safe(glVendor, safeVendor, sizeof(safeVendor));
    json_safe(glRenderer, safeRenderer, sizeof(safeRenderer));
    json_safe(glVersion, safeVersion, sizeof(safeVersion));

#if CRYSTAL_EMULATOR_TEST
    auto_spin = 0.0f;
    TraceLog(LOG_INFO, "Crystal: emulator diagnostics enabled");
#endif

    CrystalGrowth growth;
    GrowthLaw law=growth_default_law((GrowthMaterial)CRYSTAL_KIND);
    if(!growth_initialize(&growth,&law,1)) { CloseWindow(); return 2; }
#if defined(__ANDROID__)
    restore_growth(&growth);
#endif
    CrystalSurface net;
    if(!growth_surface(&growth,&net)) { CloseWindow(); return 2; }
    Mesh mesh = build_raylib_mesh(&net);
    Model model = LoadModelFromMesh(mesh);
    CrystalEdge edges[MAX_NET_EDGES];
    int edge_count = collect_boundary_edges(&net, edges);
    if (edge_count < 0 || !validate_surface(&net)) {
        TraceLog(LOG_ERROR, "Crystal: invalid geometry; rendering withheld");
        UnloadModel(model); CloseWindow(); return 2;
    }
    TraceLog(LOG_INFO, "Crystal: kind=%d nodes=%d facets=%d edges=%d",
             CRYSTAL_KIND, net.net.node_count, net.net.facet_count, edge_count);

    /* Reuse Mostow's directional normal shading, but retain Crystal's
       separate flat normals and raylib's model-normal matrix on rotation. */
    Shader facet_shader=LoadShaderFromMemory(crystal_facet_vertex_shader,crystal_facet_fragment_shader);
    int color_uniform=-1;
    bool valid_shader=IsShaderValid(facet_shader);
    if (valid_shader) color_uniform=GetShaderLocation(facet_shader,"u_material_color");
    if (!valid_shader || color_uniform<0 ||
        facet_shader.locs[SHADER_LOC_MATRIX_MVP]<0 ||
        facet_shader.locs[SHADER_LOC_MATRIX_NORMAL]<0 ||
        GetShaderLocationAttrib(facet_shader,"vertexNormal")<0) {
        TraceLog(LOG_ERROR,"Crystal facet lighting shader unavailable");
        UnloadShader(facet_shader); UnloadModel(model); CloseWindow(); return 2;
    }
    model.materials[0].shader=facet_shader;
    const float material_color[4]={tint.r/255.0f,tint.g/255.0f,tint.b/255.0f,tint.a/255.0f};
    SetShaderValue(facet_shader,color_uniform,material_color,SHADER_UNIFORM_VEC4);

    Camera3D camera = {0};
    camera.position = (Vector3){ 4.3f, 3.2f, 5.4f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 43.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float pitch = -0.18f;
    float yaw = 0.55f;
    int renderMode = 2;  // 0 = solid, 1 = net edges, 2 = both
    bool pointerWasDown = false;
    bool pointerConsumed = false;
    Vector2 previousPointer = {0};
    CrystalZoom zoom;
    crystal_zoom_reset(&zoom);
    bool frozen = CRYSTAL_EMULATOR_TEST || growth.paused;
    bool showGrid = false;
    bool xrayEdges = false;
    double simulation_accumulator=0;
    bool growth_error=false;
    bool automatic_fit=true;
    double clip_far=RL_CULL_DISTANCE_FAR;
    unsigned long frame=0;
    int lastTouch=-1, lastMode=-1;
    float lastDistance=-1.0f;
    Vector3 cameraDirection = Vector3Normalize(Vector3Subtract(camera.position, camera.target));
#if defined(__ANDROID__)
    persistent_growth=&growth; persistent_user_pause=&frozen;
    struct android_app *android_app=GetAndroidApp();
    previous_android_command=android_app->onAppCmd;
    android_app->onAppCmd=crystal_android_command;
#endif

    while (!WindowShouldClose()) {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Rectangle resetRect = { 20.0f, (float)sh - 92.0f, 150.0f, 58.0f };
        Rectangle wireRect = { (float)sw - 170.0f, (float)sh - 92.0f, 150.0f, 58.0f };
        Rectangle zoomInRect = {20.0f, (float)sh-164.0f, 76.0f, 52.0f};
        Rectangle zoomOutRect = {108.0f, (float)sh-164.0f, 76.0f, 52.0f};
        Rectangle freezeRect = {196.0f, (float)sh-164.0f, 104.0f, 52.0f};
        Rectangle gridRect = {312.0f, (float)sh-164.0f, 100.0f, 52.0f};
        Rectangle depthRect = {424.0f, (float)sh-164.0f, 130.0f, 52.0f};

        int touchCount = GetTouchPointCount();
        int observedPointer0=touchCount>0?GetTouchPointId(0):-1;
        int observedPointer1=touchCount>1?GetTouchPointId(1):-1;
        bool pointerDown = false;
        Vector2 pointer = {0};

        if (touchCount >= 2) {
            automatic_fit=false;
            Vector2 firstTouch = GetTouchPosition(0);
            Vector2 secondTouch = GetTouchPosition(1);
            float pinchDistance = Vector2Distance(firstTouch, secondTouch);

            crystal_zoom_touches(&zoom, touchCount, GetTouchPointId(0), GetTouchPointId(1), pinchDistance);
            pointerWasDown = false;
            pointerConsumed = false;
        } else {
            if (zoom.pinching) {
                pointerWasDown = false;
                pointerConsumed = false;
            }
            crystal_zoom_touches(&zoom, touchCount, -1, -1, 0.0f);

            if (touchCount == 1) {
                pointerDown = true;
                pointer = GetTouchPosition(0);
            } else if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                pointerDown = true;
                pointer = GetMousePosition();
            }

            if (pointerDown && !pointerWasDown) {
                pointerConsumed = false;
                if (CheckCollisionPointRec(pointer, resetRect)) {
                    pitch = -0.18f;
                    yaw = 0.55f;
                    crystal_zoom_reset(&zoom);
                    automatic_fit=true;
                    GrowthMetrics bounds; growth_validate(&growth,&bounds);
                    crystal_zoom_apply(&zoom,fit_distance((float)bounds.maximum_radius,sw,sh,camera.fovy));
                    pointerConsumed = true;
                } else if (CheckCollisionPointRec(pointer, wireRect)) {
                    renderMode = (renderMode + 1) % 3;
                    pointerConsumed = true;
                } else if (CheckCollisionPointRec(pointer, zoomInRect)) {
                    automatic_fit=false;
                    crystal_zoom_apply(&zoom, zoom.distance/1.25f); pointerConsumed=true;
                } else if (CheckCollisionPointRec(pointer, zoomOutRect)) {
                    automatic_fit=false;
                    crystal_zoom_apply(&zoom, zoom.distance*1.25f); pointerConsumed=true;
                } else if (CheckCollisionPointRec(pointer, freezeRect)) {
                    frozen=!frozen; pointerConsumed=true;
#if defined(__ANDROID__)
                    checkpoint_growth(android_app);
#endif
                } else if (CheckCollisionPointRec(pointer, gridRect)) {
                    if(!growth_initialize(&growth,&law,1)) growth_error=true;
                    simulation_accumulator=0; pointerConsumed=true;
                } else if (CheckCollisionPointRec(pointer, depthRect)) {
                    xrayEdges=!xrayEdges; pointerConsumed=true;
                }
                previousPointer = pointer;
            } else if (pointerDown && pointerWasDown && !pointerConsumed) {
                Vector2 delta = Vector2Subtract(pointer, previousPointer);
                yaw += delta.x * 0.010f;
                pitch += delta.y * 0.010f;
                pitch = Clamp(pitch, -1.45f, 1.45f);
                previousPointer = pointer;
            }

            pointerWasDown = pointerDown;
            if (!pointerDown) pointerConsumed = false;
        }

        growth.paused=frozen || !IsWindowFocused();
        if(!growth.paused && !growth_error) {
            simulation_accumulator+=fmin((double)GetFrameTime(),.08);
            for(int budget=0;budget<4 && simulation_accumulator>=.02;budget++) {
                if(!growth_step(&growth,.02)) { growth_error=true; break; }
                simulation_accumulator-=.02;
            }
        }
        CrystalSurface updated;
        if(!growth_surface(&growth,&updated) || !update_raylib_mesh(&model.meshes[0],&updated)) growth_error=true;
        else { net=updated; edge_count=collect_boundary_edges(&net,edges); if(edge_count<0) growth_error=true; }
        GrowthMetrics growth_metrics; growth_validate(&growth,&growth_metrics);
        /* Grow visibly at a fixed camera distance; expand the fit only when the
         * front would leave the viewport. User zoom remains available. */
        float fit=fit_distance((float)growth_metrics.maximum_radius,sw,sh,camera.fovy);
        zoom.minimum=.05f; zoom.maximum=fmaxf(18,fit*8);
        if(automatic_fit && fit>zoom.distance) crystal_zoom_apply(&zoom,fit);
        clip_far=fmax((double)RL_CULL_DISTANCE_FAR,zoom.distance+2*growth_metrics.maximum_radius);
        rlSetClipPlanes(RL_CULL_DISTANCE_NEAR,clip_far);
        if (!frozen) yaw += auto_spin * GetFrameTime();
        model.transform = MatrixMultiply(MatrixRotateX(pitch), MatrixRotateY(yaw));
        camera.position = Vector3Add(camera.target, Vector3Scale(cameraDirection, zoom.distance));

        BeginDrawing();
        ClearBackground((Color){ 18, 20, 25, 255 });

        BeginMode3D(camera);
        if (showGrid) DrawGrid(12, 0.5f);
        /* Occluded edges need the surface in the depth buffer even when its
         * pigment is hidden. Otherwise NET mode accidentally becomes x-ray. */
        if ((renderMode != 1 || !xrayEdges) && !growth_error) {
            bool depth_only=renderMode==1;
            rlDrawRenderBatchActive();
            if(depth_only) rlColorMask(false,false,false,false);
            rlDisableBackfaceCulling();
            /* The shader owns pigment and normal lighting; avoid double tinting. */
            DrawModel(model, Vector3Zero(), 1.0f, WHITE);
            rlDrawRenderBatchActive();
            rlEnableBackfaceCulling();
            if(depth_only) rlColorMask(true,true,true,true);
        }
        if (renderMode != 0 && !growth_error) {
            // Flush before changing state: deferred triangles/lines otherwise
            // inherit the depth state at flush, rather than at submission.
            rlDrawRenderBatchActive();
            if (xrayEdges) rlDisableDepthTest();
            rlSetLineWidth(2.0f);
            draw_net_edges(&net, edges, edge_count, model.transform, (Color){ 245, 248, 252, 255 });
            rlSetLineWidth(1.0f);
            rlDrawRenderBatchActive();
            if (xrayEdges) rlEnableDepthTest();
        }
        EndMode3D();

        DrawRectangle(0, 0, sw, 82, Fade(BLACK, 0.55f));
        DrawText(CRYSTAL_TITLE, 20, 18, 28, RAYWHITE);
        DrawText(TextFormat("%.12s  %s", CRYSTAL_SOURCE_COMMIT,
                 renderMode == 0 ? "SOLID" : renderMode == 1 ? "EDGES" : "SOLID+EDGES"), 20, 50, 16, LIGHTGRAY);
        DrawText(TextFormat("distance %.2f [%.1f,%.1f] clip %.2f/%.0f",
                 zoom.distance, zoom.minimum, zoom.maximum,
                 (double)RL_CULL_DISTANCE_NEAR, clip_far), 20, 88, 16, LIGHTGRAY);
        DrawText(TextFormat("touch %d ids %d/%d span %.1f zoom %.3f %s",
                 touchCount, zoom.first, zoom.second, zoom.span, zoom.applied,
                 frozen ? "FROZEN" : "SPIN"), 20, 110, 16, LIGHTGRAY);
        DrawText(TextFormat("geometry %s: %d nodes %d facets %d triangles %d edges",
                 growth_error?"REJECTED":"OK",net.net.node_count, net.net.facet_count, mesh.triangleCount, edge_count), 20, 132, 14, growth_error?RED:LIGHTGRAY);
        DrawText(TextFormat("%s / %s", glVendor, glRenderer), 20, 152, 12, LIGHTGRAY);
        DrawText(TextFormat("growth %s  step %llu  t %.2f  volume %.3f",
                 growth_error?"REJECTED":growth.paused?"PAUSED":"RUNNING",(unsigned long long)growth.steps,growth.time,growth_metrics.volume),20,174,16,growth_error?RED:LIME);

        DrawRectangleRec(resetRect, Fade(DARKGRAY, 0.92f));
        DrawRectangleLinesEx(resetRect, 2.0f, GRAY);
        DrawText("FIT", (int)resetRect.x + 31, (int)resetRect.y + 18, 20, RAYWHITE);

        DrawRectangleRec(wireRect, Fade(DARKGRAY, 0.92f));
        DrawRectangleLinesEx(wireRect, 2.0f, GRAY);
        const char *viewLabel = (renderMode == 0) ? "SOLID" : (renderMode == 1) ? "NET" : "BOTH";
        DrawText(viewLabel, (int)wireRect.x + 42, (int)wireRect.y + 18, 18, RAYWHITE);

        DrawRectangleRec(zoomInRect, DARKGRAY); DrawText("+", 48, sh-152, 24, RAYWHITE);
        DrawRectangleRec(zoomOutRect, DARKGRAY); DrawText("-", 136, sh-152, 24, RAYWHITE);
        DrawRectangleRec(freezeRect, DARKGRAY); DrawText(frozen ? "RESUME" : "PAUSE", 204, sh-146, 16, RAYWHITE);
        DrawRectangleRec(gridRect, DARKGRAY); DrawText("SEED", 326, sh-146, 16, RAYWHITE);
        DrawRectangleRec(depthRect, DARKGRAY); DrawText(xrayEdges ? "X-RAY" : "OCCLUDED", 432, sh-146, 14, RAYWHITE);
        DrawText("drag: rotate   pinch / +/-: zoom", 20, sh - 190, 17, LIGHTGRAY);

#if CRYSTAL_EMULATOR_TEST
        DrawRectangle(0, 88, sw, 94, Fade(BLACK, 0.60f));
        DrawText(TextFormat("CI diag: kind=%d nodes=%d facets=%d edges=%d",
                            CRYSTAL_KIND, net.net.node_count, net.net.facet_count, edge_count),
                 20, 96, 16, LIME);
        DrawText(TextFormat("mode=%s yaw=%.3f pitch=%.3f dist=%.3f",
                            viewLabel, yaw, pitch, zoom.distance),
                 20, 122, 16, LIME);
        DrawText(TextFormat("screen=%dx%d projection=perspective", sw, sh),
                 20, 148, 16, LIME);
#endif

        EndDrawing();
        frame++;
        if (touchCount != lastTouch || renderMode != lastMode ||
            fabsf(zoom.distance-lastDistance)>0.001f || frame%60==0) {
            char snapshot[4096];
            snprintf(snapshot, sizeof(snapshot),
                "{\"schema\":\"crystal-state-v1\",\"source_commit\":\"%s\",\"package\":\"%s\","
                "\"session\":\"%s\",\"replay_id\":\"frame-%lu\",\"screen_width\":\"%d\",\"screen_height\":\"%d\",\"gl_vendor\":\"%s\","
                "\"gl_renderer\":\"%s\",\"gl_version\":\"%s\",\"render_path\":\"gles2-explicit-lines\","
                "\"geometry_result\":\"%s\",\"growth_steps\":\"%llu\",\"growth_time\":\"%.17g\","
                "\"growth_volume\":\"%.17g\",\"growth_paused\":\"%d\","
                "\"camera_distance\":\"%.5f\",\"camera_min\":\"%.3f\","
                "\"camera_max\":\"%.3f\",\"clip_near\":\"%.3f\",\"clip_far\":\"%.3f\","
                "\"touch_count\":\"%d\",\"pointer_ids\":\"%d,%d\",\"pinch_span\":\"%.5f\","
                "\"applied_zoom\":\"%.5f\",\"requested_distance\":\"%.5f\",\"yaw\":\"%.5f\","
                "\"pitch\":\"%.5f\",\"frozen\":\"%d\",\"render_mode\":\"%d\",\"xray\":\"%d\"}",
                CRYSTAL_SOURCE_COMMIT, CRYSTAL_PACKAGE_ID, launchSession, frame, sw, sh,
                safeVendor, safeRenderer, safeVersion, growth_error?"FAIL":"PASS",
                (unsigned long long)growth.steps,growth.time,growth_metrics.volume,growth.paused,
                zoom.distance, zoom.minimum, zoom.maximum,
                (double)RL_CULL_DISTANCE_NEAR, clip_far, touchCount,
                observedPointer0, observedPointer1, zoom.span, zoom.applied, zoom.requested, yaw, pitch,
                frozen, renderMode, xrayEdges);
#if defined(__ANDROID__)
            __android_log_write(ANDROID_LOG_INFO, "CrystalState", snapshot);
#else
            TraceLog(LOG_INFO, "CrystalState: %s", snapshot);
#endif
            lastTouch=touchCount; lastMode=renderMode; lastDistance=zoom.distance;
        }
    }

#if defined(__ANDROID__)
    checkpoint_growth(android_app);
    android_app->onAppCmd=previous_android_command;
    persistent_growth=NULL; persistent_user_pause=NULL;
#endif
    UnloadModel(model);
    UnloadShader(facet_shader);
    CloseWindow();
    return 0;
}
#endif

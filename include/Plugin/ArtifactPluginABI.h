#ifndef ARTIFACT_PLUGIN_ABI_H
#define ARTIFACT_PLUGIN_ABI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ARTIFACT_PLUGIN_API_VERSION 1

#ifdef _WIN32
    #ifdef ARTIFACT_PLUGIN_DLL_EXPORT
        #define ARTIFACT_PLUGIN_API __declspec(dllexport)
    #else
        #define ARTIFACT_PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define ARTIFACT_PLUGIN_API __attribute__((visibility("default")))
#endif

typedef enum {
    ARTIFACT_PLUGIN_CATEGORY_EFFECT       = 0,
    ARTIFACT_PLUGIN_CATEGORY_LAYER        = 1,
    ARTIFACT_PLUGIN_CATEGORY_TOOL         = 2,
    ARTIFACT_PLUGIN_CATEGORY_IMPORT_EXPORT = 3
} ArtifactPluginCategory;

typedef struct {
    const char* id;
    const char* displayName;
    const char* version;
    const char* author;
    const char* description;
    ArtifactPluginCategory category;
    int apiVersion;
} ArtifactPluginDescriptor;

typedef void* ArtifactPluginInstance;

typedef struct {
    const char* (*getId)(ArtifactPluginInstance);
    const char* (*getDisplayName)(ArtifactPluginInstance);
    int (*initialize)(ArtifactPluginInstance);
    void (*shutdown)(ArtifactPluginInstance);
    void (*drawContent)(ArtifactPluginInstance,
                        const void* layerPtr,
                        float currentTime,
                        int frameNumber,
                        int compWidth,
                        int compHeight);
    int (*serializeExtra)(ArtifactPluginInstance,
                          const void* layerPtr,
                          char** jsonOut);
    int (*deserializeExtra)(ArtifactPluginInstance,
                            void* layerPtr,
                            const char* jsonIn);
    int (*getPropertyGroupCount)(ArtifactPluginInstance);
    int (*getPropertyGroupDef)(ArtifactPluginInstance,
                               int index,
                               char** nameOut,
                               char** jsonSchemaOut);
} ArtifactLayerPluginVTable;

typedef struct {
    const char* (*getId)(ArtifactPluginInstance);
    const char* (*getDisplayName)(ArtifactPluginInstance);
    const char* (*getDefaultShortcut)(ArtifactPluginInstance);
    int (*activate)(ArtifactPluginInstance);
    void (*deactivate)(ArtifactPluginInstance);
    int (*onMousePress)(ArtifactPluginInstance,
                        int x, int y,
                        int buttons, int modifiers,
                        int64_t timestampMs);
    int (*onMouseMove)(ArtifactPluginInstance,
                       int x, int y,
                       int buttons, int modifiers,
                       int64_t timestampMs);
    int (*onMouseRelease)(ArtifactPluginInstance,
                          int x, int y,
                          int buttons, int modifiers,
                          int64_t timestampMs);
    int (*onKeyPress)(ArtifactPluginInstance,
                      int keyCode, int modifiers,
                      int isAutoRepeat);
    int (*onKeyRelease)(ArtifactPluginInstance,
                        int keyCode, int modifiers);
    int (*getCursorShape)(ArtifactPluginInstance);
} ArtifactToolPluginVTable;

ARTIFACT_PLUGIN_API int ArtifactPlugin_GetAPIVersion(void);
ARTIFACT_PLUGIN_API int ArtifactPlugin_GetPluginCount(void);
ARTIFACT_PLUGIN_API const ArtifactPluginDescriptor* ArtifactPlugin_GetPlugin(int index);

ARTIFACT_PLUGIN_API ArtifactPluginInstance ArtifactPlugin_CreateLayer(const char* id);
ARTIFACT_PLUGIN_API void ArtifactPlugin_DestroyLayer(ArtifactPluginInstance instance);
ARTIFACT_PLUGIN_API const ArtifactLayerPluginVTable* ArtifactPlugin_GetLayerVTable(const char* id);

ARTIFACT_PLUGIN_API ArtifactPluginInstance ArtifactPlugin_CreateTool(const char* id);
ARTIFACT_PLUGIN_API void ArtifactPlugin_DestroyTool(ArtifactPluginInstance instance);
ARTIFACT_PLUGIN_API const ArtifactToolPluginVTable* ArtifactPlugin_GetToolVTable(const char* id);

#ifdef __cplusplus
}
#endif

#endif

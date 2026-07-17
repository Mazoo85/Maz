#ifndef MAZ_PLUGIN_API_H
#define MAZ_PLUGIN_API_H

/* CJC Music Station native audio-plugin ABI.
 *
 * A plugin is a shared library (.so/.dll/.dylib) that exports the C entry points below. The host
 * (maz::audio::PluginHost) dlopen's it and runs it as an effect in the mixer chain. This is a small,
 * stable C ABI — a VST3/CLAP shim can be written against it later, but it already provides real
 * dynamically-loaded plugin hosting. */

#ifdef __cplusplus
extern "C" {
#endif

#define MAZ_PLUGIN_ABI_VERSION 1

/* Opaque per-plugin instance, defined by the plugin. */
typedef struct MazPluginInstance MazPluginInstance;

typedef struct MazPluginDesc {
    int abi_version;  /* must equal MAZ_PLUGIN_ABI_VERSION */
    const char* name; /* human-readable effect name */
    int param_count;  /* number of automatable parameters */
} MazPluginDesc;

/* Exported symbols a plugin MUST provide (exact names):
 *
 *   const MazPluginDesc* maz_plugin_descriptor(void);
 *   MazPluginInstance*   maz_plugin_create(int sample_rate);
 *   void                 maz_plugin_process(MazPluginInstance*, float* stereo, int frames);
 *   void                 maz_plugin_set_param(MazPluginInstance*, int index, float value);
 *   void                 maz_plugin_destroy(MazPluginInstance*);
 *
 * `stereo` is interleaved L/R, processed in place. */

typedef const MazPluginDesc* (*MazPluginDescriptorFn)(void);
typedef MazPluginInstance* (*MazPluginCreateFn)(int);
typedef void (*MazPluginProcessFn)(MazPluginInstance*, float*, int);
typedef void (*MazPluginSetParamFn)(MazPluginInstance*, int, float);
typedef void (*MazPluginDestroyFn)(MazPluginInstance*);

#ifdef __cplusplus
}
#endif

#endif /* MAZ_PLUGIN_API_H */

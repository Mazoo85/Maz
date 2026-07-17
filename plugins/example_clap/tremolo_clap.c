/* Example CLAP plugin: a stereo tremolo. A minimal but valid CLAP plugin (clap_entry → factory →
 * plugin with stereo audio ports + process) used to exercise the ClapHost. */

#include <clap/clap.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    clap_plugin_t plugin;
    double sample_rate;
    float depth;
    float rate;
    float phase;
} tremolo_t;

/* ---- audio-ports extension: 1 stereo input, 1 stereo output ---- */

static uint32_t ap_count(const clap_plugin_t* plugin, bool is_input) {
    (void)plugin;
    (void)is_input;
    return 1;
}

static bool ap_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                   clap_audio_port_info_t* info) {
    (void)plugin;
    if (index != 0) {
        return false;
    }
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "%s", is_input ? "In" : "Out");
    info->channel_count = 2;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}

static const clap_plugin_audio_ports_t s_audio_ports = {ap_count, ap_get};

/* ---- plugin ---- */

static bool plug_init(const clap_plugin_t* plugin) {
    (void)plugin;
    return true;
}
static void plug_destroy(const clap_plugin_t* plugin) {
    /* plugin_data points at the tremolo_t whose first member is this clap_plugin. */
    free(plugin->plugin_data);
}
static bool plug_activate(const clap_plugin_t* plugin, double sr, uint32_t minf, uint32_t maxf) {
    (void)minf;
    (void)maxf;
    tremolo_t* t = (tremolo_t*)plugin->plugin_data;
    t->sample_rate = sr > 0 ? sr : 48000.0;
    t->phase = 0.0f;
    return true;
}
static void plug_deactivate(const clap_plugin_t* plugin) {
    (void)plugin;
}
static bool plug_start(const clap_plugin_t* plugin) {
    (void)plugin;
    return true;
}
static void plug_stop(const clap_plugin_t* plugin) {
    (void)plugin;
}
static void plug_reset(const clap_plugin_t* plugin) {
    tremolo_t* t = (tremolo_t*)plugin->plugin_data;
    t->phase = 0.0f;
}

static clap_process_status plug_process(const clap_plugin_t* plugin, const clap_process_t* p) {
    tremolo_t* t = (tremolo_t*)plugin->plugin_data;
    if (p->audio_inputs_count < 1 || p->audio_outputs_count < 1) {
        return CLAP_PROCESS_CONTINUE;
    }
    float* inL = p->audio_inputs[0].data32[0];
    float* inR = p->audio_inputs[0].data32[1];
    float* outL = p->audio_outputs[0].data32[0];
    float* outR = p->audio_outputs[0].data32[1];
    const float twoPi = 6.2831853f;
    for (uint32_t i = 0; i < p->frames_count; ++i) {
        const float lfo = 0.5f + 0.5f * sinf(t->phase * twoPi);
        const float g = 1.0f - t->depth * lfo;
        outL[i] = inL[i] * g;
        outR[i] = inR[i] * g;
        t->phase += t->rate / (float)t->sample_rate;
        if (t->phase >= 1.0f) {
            t->phase -= 1.0f;
        }
    }
    return CLAP_PROCESS_CONTINUE;
}

static const void* plug_get_extension(const clap_plugin_t* plugin, const char* id) {
    (void)plugin;
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &s_audio_ports;
    }
    return NULL;
}
static void plug_on_main_thread(const clap_plugin_t* plugin) {
    (void)plugin;
}

static const char* s_features[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT, NULL};

static const clap_plugin_descriptor_t s_desc = {
    CLAP_VERSION_INIT,
    "com.cjc.tremolo",
    "CJC CLAP Tremolo",
    "CJC",
    "https://example.invalid",
    "",
    "",
    "1.0.0",
    "A stereo tremolo, in CLAP.",
    s_features,
};

/* ---- factory ---- */

static uint32_t factory_count(const clap_plugin_factory_t* f) {
    (void)f;
    return 1;
}
static const clap_plugin_descriptor_t* factory_desc(const clap_plugin_factory_t* f, uint32_t index) {
    (void)f;
    return index == 0 ? &s_desc : NULL;
}
static const clap_plugin_t* factory_create(const clap_plugin_factory_t* f, const clap_host_t* host,
                                           const char* plugin_id) {
    (void)f;
    (void)host;
    if (plugin_id == NULL || strcmp(plugin_id, s_desc.id) != 0) {
        return NULL;
    }
    tremolo_t* t = (tremolo_t*)calloc(1, sizeof(tremolo_t));
    if (t == NULL) {
        return NULL;
    }
    t->depth = 0.8f;
    t->rate = 5.0f;
    t->sample_rate = 48000.0;
    t->plugin.desc = &s_desc;
    t->plugin.plugin_data = t;
    t->plugin.init = plug_init;
    t->plugin.destroy = plug_destroy;
    t->plugin.activate = plug_activate;
    t->plugin.deactivate = plug_deactivate;
    t->plugin.start_processing = plug_start;
    t->plugin.stop_processing = plug_stop;
    t->plugin.reset = plug_reset;
    t->plugin.process = plug_process;
    t->plugin.get_extension = plug_get_extension;
    t->plugin.on_main_thread = plug_on_main_thread;
    return &t->plugin;
}

static const clap_plugin_factory_t s_factory = {factory_count, factory_desc, factory_create};

/* ---- entry ---- */

static bool entry_init(const char* path) {
    (void)path;
    return true;
}
static void entry_deinit(void) {}
static const void* entry_get_factory(const char* factory_id) {
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &s_factory;
    }
    return NULL;
}

CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    entry_init,
    entry_deinit,
    entry_get_factory,
};

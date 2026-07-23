/* Example CLAP *instrument* plugin: a minimal monophonic sine synth. Unlike the tremolo (an audio
 * effect), this declares a note input port + a stereo audio OUTPUT (no audio input) and synthesises
 * sound from CLAP note events — the shape the ClapHost's instrument-hosting path drives. Kept
 * deliberately small (last-note mono, gated sine) to exercise the host, not to sound great. */

#include <clap/clap.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    clap_plugin_t plugin;
    double sample_rate;
    double phase;
    float freq;  /* current note frequency (Hz) */
    float gate;  /* 1 while a note is held, 0 otherwise */
    int key;     /* MIDI key of the held note, or -1 */
    float gain;  /* param 0: output gain (0..1), automatable by the host */
} synth_t;

/* ---- audio-ports extension: 0 inputs, 1 stereo output ---- */

static uint32_t ap_count(const clap_plugin_t* plugin, bool is_input) {
    (void)plugin;
    return is_input ? 0u : 1u;
}
static bool ap_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                   clap_audio_port_info_t* info) {
    (void)plugin;
    if (is_input || index != 0) {
        return false;
    }
    info->id = 0;
    snprintf(info->name, sizeof(info->name), "%s", "Out");
    info->channel_count = 2;
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}
static const clap_plugin_audio_ports_t s_audio_ports = {ap_count, ap_get};

/* ---- note-ports extension: 1 input note port (this is what marks it an instrument) ---- */

static uint32_t np_count(const clap_plugin_t* plugin, bool is_input) {
    (void)plugin;
    return is_input ? 1u : 0u;
}
static bool np_get(const clap_plugin_t* plugin, uint32_t index, bool is_input,
                   clap_note_port_info_t* info) {
    (void)plugin;
    if (!is_input || index != 0) {
        return false;
    }
    info->id = 0;
    info->supported_dialects = CLAP_NOTE_DIALECT_CLAP;
    info->preferred_dialect = CLAP_NOTE_DIALECT_CLAP;
    snprintf(info->name, sizeof(info->name), "%s", "Notes");
    return true;
}
static const clap_plugin_note_ports_t s_note_ports = {np_count, np_get};

/* ---- params extension: one automatable "Gain" parameter (id 0) ---- */

static uint32_t pr_count(const clap_plugin_t* plugin) {
    (void)plugin;
    return 1u;
}
static bool pr_get_info(const clap_plugin_t* plugin, uint32_t index, clap_param_info_t* info) {
    (void)plugin;
    if (index != 0) {
        return false;
    }
    memset(info, 0, sizeof(*info));
    info->id = 0;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 1.0;
    snprintf(info->name, sizeof(info->name), "%s", "Gain");
    return true;
}
static bool pr_get_value(const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    synth_t* s = (synth_t*)plugin->plugin_data;
    if (param_id != 0) {
        return false;
    }
    *out_value = (double)s->gain;
    return true;
}
static bool pr_value_to_text(const clap_plugin_t* plugin, clap_id param_id, double value, char* out,
                             uint32_t size) {
    (void)plugin;
    if (param_id != 0) {
        return false;
    }
    snprintf(out, size, "%.2f", value);
    return true;
}
static bool pr_text_to_value(const clap_plugin_t* plugin, clap_id param_id, const char* text,
                             double* out) {
    (void)plugin;
    if (param_id != 0 || text == NULL) {
        return false;
    }
    *out = atof(text);
    return true;
}
static void apply_param_event(synth_t* s, const clap_event_header_t* h) {
    if (h->space_id == CLAP_CORE_EVENT_SPACE_ID && h->type == CLAP_EVENT_PARAM_VALUE) {
        const clap_event_param_value_t* pv = (const clap_event_param_value_t*)h;
        if (pv->param_id == 0) {
            s->gain = (float)pv->value;
        }
    }
}
static void pr_flush(const clap_plugin_t* plugin, const clap_input_events_t* in,
                     const clap_output_events_t* out) {
    (void)out;
    synth_t* s = (synth_t*)plugin->plugin_data;
    if (in != NULL && in->size != NULL && in->get != NULL) {
        const uint32_t n = in->size(in);
        for (uint32_t i = 0; i < n; ++i) {
            const clap_event_header_t* h = in->get(in, i);
            if (h != NULL) {
                apply_param_event(s, h);
            }
        }
    }
}
static const clap_plugin_params_t s_params = {pr_count,         pr_get_info,     pr_get_value,
                                              pr_value_to_text, pr_text_to_value, pr_flush};

/* ---- plugin ---- */

static bool plug_init(const clap_plugin_t* plugin) {
    (void)plugin;
    return true;
}
static void plug_destroy(const clap_plugin_t* plugin) {
    free(plugin->plugin_data);
}
static bool plug_activate(const clap_plugin_t* plugin, double sr, uint32_t minf, uint32_t maxf) {
    (void)minf;
    (void)maxf;
    synth_t* s = (synth_t*)plugin->plugin_data;
    s->sample_rate = sr > 0 ? sr : 48000.0;
    s->phase = 0.0;
    s->gate = 0.0f;
    s->key = -1;
    return true;
}
/* (gain persists across activate so a host-set value isn't reset; defaults to 1 at create.) */
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
    synth_t* s = (synth_t*)plugin->plugin_data;
    s->phase = 0.0;
    s->gate = 0.0f;
    s->key = -1;
}

static void handle_event(synth_t* s, const clap_event_header_t* h) {
    if (h->space_id != CLAP_CORE_EVENT_SPACE_ID) {
        return;
    }
    if (h->type == CLAP_EVENT_PARAM_VALUE) {
        apply_param_event(s, h);
        return;
    }
    if (h->type == CLAP_EVENT_NOTE_ON) {
        const clap_event_note_t* n = (const clap_event_note_t*)h;
        s->key = n->key;
        s->freq = (float)(440.0 * pow(2.0, (n->key - 69) / 12.0));
        s->gate = 1.0f;
    } else if (h->type == CLAP_EVENT_NOTE_OFF) {
        const clap_event_note_t* n = (const clap_event_note_t*)h;
        if (n->key < 0 || n->key == s->key) {
            s->gate = 0.0f;
            s->key = -1;
        }
    }
}

static clap_process_status plug_process(const clap_plugin_t* plugin, const clap_process_t* p) {
    synth_t* s = (synth_t*)plugin->plugin_data;
    if (p->audio_outputs_count < 1) {
        return CLAP_PROCESS_CONTINUE;
    }
    /* Apply all incoming note events for this block up front (sample-accurate timing is not needed to
     * exercise the host). */
    if (p->in_events != NULL && p->in_events->size != NULL && p->in_events->get != NULL) {
        const uint32_t n = p->in_events->size(p->in_events);
        for (uint32_t i = 0; i < n; ++i) {
            const clap_event_header_t* h = p->in_events->get(p->in_events, i);
            if (h != NULL) {
                handle_event(s, h);
            }
        }
    }
    float* outL = p->audio_outputs[0].data32[0];
    float* outR = p->audio_outputs[0].data32[1];
    const double twoPi = 6.283185307179586;
    for (uint32_t i = 0; i < p->frames_count; ++i) {
        const float v = (float)(sin(s->phase * twoPi)) * s->gate * 0.3f * s->gain;
        outL[i] = v;
        outR[i] = v;
        s->phase += (double)s->freq / s->sample_rate;
        if (s->phase >= 1.0) {
            s->phase -= 1.0;
        }
    }
    return CLAP_PROCESS_CONTINUE;
}

static const void* plug_get_extension(const clap_plugin_t* plugin, const char* id) {
    (void)plugin;
    if (strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &s_audio_ports;
    }
    if (strcmp(id, CLAP_EXT_NOTE_PORTS) == 0) {
        return &s_note_ports;
    }
    if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &s_params;
    }
    return NULL;
}
static void plug_on_main_thread(const clap_plugin_t* plugin) {
    (void)plugin;
}

static const char* s_features[] = {CLAP_PLUGIN_FEATURE_INSTRUMENT, CLAP_PLUGIN_FEATURE_SYNTHESIZER,
                                   NULL};

static const clap_plugin_descriptor_t s_desc = {
    CLAP_VERSION_INIT,
    "com.cjc.synth",
    "CJC CLAP Synth",
    "CJC",
    "https://example.invalid",
    "",
    "",
    "1.0.0",
    "A monophonic sine instrument, in CLAP.",
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
    synth_t* s = (synth_t*)calloc(1, sizeof(synth_t));
    if (s == NULL) {
        return NULL;
    }
    s->sample_rate = 48000.0;
    s->freq = 440.0f;
    s->key = -1;
    s->gain = 1.0f;
    s->plugin.desc = &s_desc;
    s->plugin.plugin_data = s;
    s->plugin.init = plug_init;
    s->plugin.destroy = plug_destroy;
    s->plugin.activate = plug_activate;
    s->plugin.deactivate = plug_deactivate;
    s->plugin.start_processing = plug_start;
    s->plugin.stop_processing = plug_stop;
    s->plugin.reset = plug_reset;
    s->plugin.process = plug_process;
    s->plugin.get_extension = plug_get_extension;
    s->plugin.on_main_thread = plug_on_main_thread;
    return &s->plugin;
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

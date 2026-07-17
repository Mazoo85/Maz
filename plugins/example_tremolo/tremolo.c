/* Example CJC Music Station native plugin: a stereo tremolo (amplitude modulation).
 * Built as a shared library and dlopen'd by the host — a reference implementation of the plugin ABI
 * in maz/audio/PluginApi.h. */

#include "maz/audio/PluginApi.h"

#include <math.h>
#include <stdlib.h>

struct MazPluginInstance {
    float depth; /* param 0: 0..1 */
    float rate;  /* param 1: Hz   */
    float phase;
    int sampleRate;
};

static const MazPluginDesc kDesc = {MAZ_PLUGIN_ABI_VERSION, "Example Tremolo", 2};

const MazPluginDesc* maz_plugin_descriptor(void) {
    return &kDesc;
}

MazPluginInstance* maz_plugin_create(int sample_rate) {
    struct MazPluginInstance* p = (struct MazPluginInstance*)calloc(1, sizeof(struct MazPluginInstance));
    if (p == NULL) {
        return NULL;
    }
    p->depth = 0.8f;
    p->rate = 5.0f;
    p->phase = 0.0f;
    p->sampleRate = sample_rate > 0 ? sample_rate : 48000;
    return p;
}

void maz_plugin_set_param(MazPluginInstance* p, int index, float value) {
    if (p == NULL) {
        return;
    }
    if (index == 0) {
        p->depth = value;
    } else if (index == 1) {
        p->rate = value;
    }
}

void maz_plugin_process(MazPluginInstance* p, float* stereo, int frames) {
    if (p == NULL) {
        return;
    }
    const float twoPi = 6.2831853f;
    for (int i = 0; i < frames; ++i) {
        const float lfo = 0.5f + 0.5f * sinf(p->phase * twoPi);
        const float gain = 1.0f - p->depth * lfo;
        stereo[2 * i] *= gain;
        stereo[2 * i + 1] *= gain;
        p->phase += p->rate / (float)p->sampleRate;
        if (p->phase >= 1.0f) {
            p->phase -= 1.0f;
        }
    }
}

void maz_plugin_destroy(MazPluginInstance* p) {
    free(p);
}

#include "py32emu/sim/devices/speaker.h"

#include <string.h>

static void speaker_reset(SimDevice *device)
{
    SimSpeaker *speaker = device->state;
    speaker->positive = SIM_LEVEL_Z;
    speaker->negative = SIM_LEVEL_Z;
    speaker->elapsed_cycles = 0u;
    speaker->accumulator = 0;
    speaker->moment = speaker->previous_moment = 0.0;
    speaker->samples = 0u;
    speaker->cycles_per_second = 0u;
    speaker->sample_interval = 0u;
}

static void speaker_pin_changed(SimDevice *device, unsigned pin,
                                SimLevel level)
{
    SimSpeaker *speaker = device->state;
    if (pin == 0u) speaker->negative = level;
    else if (pin == 1u) speaker->positive = level;
}

static void speaker_tick(SimDevice *device, uint64_t cycles,
                         uint32_t cycles_per_second)
{
    SimSpeaker *speaker = device->state;
    int64_t difference = 0;
    uint64_t duration;
    if (speaker->sample_rate_hz == 0u || cycles_per_second == 0u) return;
    /* 用采样率倍的周期表示相位，保留非整数采样边界。 */
    if (speaker->cycles_per_second != cycles_per_second) {
        speaker->cycles_per_second = cycles_per_second;
        speaker->sample_interval = cycles_per_second;
        speaker->elapsed_cycles = 0u;
        speaker->accumulator = 0;
        speaker->moment = speaker->previous_moment = 0.0;
    }
    if (speaker->positive == SIM_LEVEL_HIGH) ++difference;
    if (speaker->negative == SIM_LEVEL_HIGH) --difference;
    duration = cycles * speaker->sample_rate_hz;
    while (duration != 0u) {
        uint64_t remaining = speaker->sample_interval - speaker->elapsed_cycles;
        uint64_t part = duration < remaining ? duration : remaining;
        if (speaker->filter_enabled)
            speaker->moment += (double)difference * (double)part *
                               (2.0 * (double)speaker->elapsed_cycles + (double)part);
        speaker->accumulator += difference * (int64_t)part;
        speaker->elapsed_cycles += part;
        duration -= part;
        if (speaker->elapsed_cycles != speaker->sample_interval) break;
        /* 严格切开边界，后一个采样窗不能污染前一个窗的积分。 */
        int64_t sample = speaker->accumulator * 32768 /
                         (int64_t)speaker->sample_interval;
        if (speaker->filter_enabled) {
            /* 两个相邻采样窗的连续三角积分，在抽取前抑制载波，
             * 不是对已经发生混叠的 PCM 做事后降噪；群延迟为一个采样周期。 */
            double interval = (double)speaker->sample_interval;
            sample = (int64_t)((speaker->previous_moment +
                      2.0 * interval * (double)speaker->accumulator - speaker->moment)
                      / interval * (16384.0 / interval));
            speaker->previous_moment = speaker->moment;
            speaker->moment = 0.0;
        }
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        if (speaker->sink != NULL)
            speaker->sink(speaker->sink_context, (int16_t)sample);
        ++speaker->samples;
        speaker->elapsed_cycles = 0u;
        speaker->accumulator = 0;
    }
}

static const SimDeviceOps SPEAKER_OPS = {
    speaker_reset,
    speaker_tick,
    speaker_pin_changed
};

void sim_speaker_init(SimSpeaker *speaker, const char *name,
                      Py32SpeakerSink sink, void *context,
                      uint32_t sample_rate_hz)
{
    memset(speaker, 0, sizeof(*speaker));
    sim_device_init(&speaker->base, name, &SPEAKER_OPS, speaker, 2u);
    speaker->sink = sink;
    speaker->sink_context = context;
    speaker->sample_rate_hz = sample_rate_hz;
    speaker->filter_enabled = true;
    speaker_reset(&speaker->base);
}

void sim_speaker_set_filter(SimSpeaker *speaker, bool enabled)
{
    if (speaker == NULL || speaker->filter_enabled == enabled) return;
    speaker->filter_enabled = enabled;
    speaker->elapsed_cycles = 0u;
    speaker->accumulator = 0;
    speaker->moment = speaker->previous_moment = 0.0;
}

void sim_speaker_set_sink(SimSpeaker *speaker, Py32SpeakerSink sink,
                          void *context)
{
    if (speaker == NULL) return;
    speaker->sink = sink;
    speaker->sink_context = context;
}

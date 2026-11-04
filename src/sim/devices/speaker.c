#include "py32emu/sim/devices/speaker.h"

#include <string.h>

static void speaker_reset(SimDevice *device)
{
    SimSpeaker *speaker = device->state;
    speaker->positive = SIM_LEVEL_Z;
    speaker->negative = SIM_LEVEL_Z;
    speaker->elapsed_cycles = 0u;
    speaker->accumulator = 0;
    speaker->samples = 0u;
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
    uint64_t interval;
    if (speaker->sample_rate_hz == 0u) return;
    /* 采样间隔只在时基变化时重算，避免每条指令执行一次整数除法。 */
    if (speaker->cycles_per_second != cycles_per_second) {
        speaker->cycles_per_second = cycles_per_second;
        speaker->sample_interval = cycles_per_second /
                                   speaker->sample_rate_hz;
        if (speaker->sample_interval == 0u) speaker->sample_interval = 1u;
    }
    interval = speaker->sample_interval;
    if (speaker->positive == SIM_LEVEL_HIGH) ++difference;
    if (speaker->negative == SIM_LEVEL_HIGH) --difference;
    speaker->accumulator += difference * (int64_t)cycles;
    speaker->elapsed_cycles += cycles;
    /*
     * 每个音频周期对差分电平积分一次，等价于一个理想低通/抽取器。
     * PWM 载波远高于采样率，平均占空比就是 PCM 样本。
     */
    while (speaker->elapsed_cycles >= interval) {
        int64_t sample = speaker->accumulator * 32768 /
                         (int64_t)speaker->elapsed_cycles;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        if (speaker->sink != NULL)
            speaker->sink(speaker->sink_context, (int16_t)sample);
        ++speaker->samples;
        /*
         * 保留超出一个采样周期的余量，避免按 16 周期批量 tick 时
         * 每次都丢弃尾部时间而造成采样率漂移。
         */
        speaker->elapsed_cycles -= interval;
        speaker->accumulator = difference * (int64_t)speaker->elapsed_cycles;
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
    speaker_reset(&speaker->base);
}

void sim_speaker_set_sink(SimSpeaker *speaker, Py32SpeakerSink sink,
                          void *context)
{
    if (speaker == NULL) return;
    speaker->sink = sink;
    speaker->sink_context = context;
}

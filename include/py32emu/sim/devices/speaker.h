#ifndef PY32EMU_SIM_DEVICES_SPEAKER_H
#define PY32EMU_SIM_DEVICES_SPEAKER_H

#include "py32emu/sim/device.h"

#include <stdint.h>

/*
 * 虚拟扬声器是板级器件：pin 0 接差分负端（PA0/TIM1_CH3），pin 1 接正端
 * （PA1/TIM1_CH4）。它按固定音频周期对两脚电平差做积分，重建 PCM 后交给
 * 前端 sink；不读取 TIM 寄存器，也不依赖 CLI/Web。
 */
typedef void (*Py32SpeakerSink)(void *context, int16_t sample);

typedef struct {
    SimDevice base;
    Py32SpeakerSink sink;
    void *sink_context;
    uint32_t sample_rate_hz;
    uint32_t cycles_per_second;
    uint64_t sample_interval;
    SimLevel positive, negative;
    uint64_t elapsed_cycles;
    int64_t accumulator;
    uint64_t samples;
} SimSpeaker;

void sim_speaker_init(SimSpeaker *speaker, const char *name,
                      Py32SpeakerSink sink, void *context,
                      uint32_t sample_rate_hz);
void sim_speaker_set_sink(SimSpeaker *speaker, Py32SpeakerSink sink,
                          void *context);

#endif

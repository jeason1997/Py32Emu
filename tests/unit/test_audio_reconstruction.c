#include "py32emu/sim/board.h"
#include "py32emu/sim/devices/speaker.h"
#include "py32emu/sim/mcu/py32.h"

#include <assert.h>
#include <stdio.h>

static unsigned samples;
static int16_t last;
static unsigned quality_samples;
static int quality_peak_error;

static void capture_quality(void *context, int16_t sample)
{
    int error = sample - 8192;
    (void)context;
    if (error < 0) error = -error;
    if (quality_samples++ > 16u && error > quality_peak_error)
        quality_peak_error = error;
}

static void capture(void *context, int16_t sample)
{
    (void)context;
    ++samples;
    last = sample;
}

static void test_pulses_inside_multiply(unsigned period, unsigned ccr3,
                                       unsigned ccr4, int16_t expected)
{
    uint8_t firmware[16] = {0, 12, 0, 32, 9, 0, 0, 8, 0x48, 0x43, 0xFD, 0xE7};
    Py32FirmwareImage image = {0};
    Py32Soc soc;
    Py32Mcu mcu;
    SimBoard board;
    SimSpeaker speaker;
    char error[128];
    int negative, positive;
    image.data = firmware;
    image.size = sizeof(firmware);
    image.load_address = 0x08000000u;
    py32_soc_init(&soc);
    assert(py32_soc_configure(&soc, py32f002ax5_description(), &image,
                              error, sizeof(error)));
    py32_mcu_init(&mcu, &soc, period);
    sim_board_init(&board, &mcu.base);
    sim_speaker_init(&speaker, "speaker", capture, NULL, 1u);
    sim_speaker_set_filter(&speaker, false);
    negative = sim_board_add_net(&board, "negative");
    positive = sim_board_add_net(&board, "positive");
    assert(sim_board_connect_mcu(&board, negative, 0u));
    assert(sim_board_connect_mcu(&board, positive, 1u));
    assert(sim_board_connect_device(&board, negative, &speaker.base, 0u));
    assert(sim_board_connect_device(&board, positive, &speaker.base, 1u));
    sim_board_reset(&board);
    assert(py32_bus_write(&soc.bus, 0x40021034u, 4u, 1u));
    assert(py32_bus_write(&soc.bus, 0x40021040u, 4u, 1u << 11));
    assert(py32_bus_write(&soc.bus, 0x50000000u, 4u, 0xAu));
    assert(py32_bus_write(&soc.bus, 0x50000020u, 4u, 0xDDu));
    assert(py32_bus_write(&soc.bus, 0x40012C2Cu, 4u, period == 32u ? 1u : 255u));
    assert(py32_bus_write(&soc.bus, 0x40012C3Cu, 4u, ccr3));
    assert(py32_bus_write(&soc.bus, 0x40012C40u, 4u, ccr4));
    assert(py32_bus_write(&soc.bus, 0x40012C1Cu, 4u, 0x6868u));
    assert(py32_bus_write(&soc.bus, 0x40012C14u, 4u, 1u));
    assert(py32_bus_write(&soc.bus, 0x40012C20u, 4u, (1u << 8) | (1u << 12)));
    assert(py32_bus_write(&soc.bus, 0x40012C44u, 4u, 1u << 15));
    assert(py32_bus_write(&soc.bus, 0x40012C00u, 4u, 1u));
    sim_board_resolve(&board);
    samples = 0u;
    for (unsigned i = 0u; samples == 0u && i < 100u; ++i)
        sim_board_step(&board);
    if (samples != 1u || last != expected)
        fprintf(stderr, "period=%u samples=%u actual=%d expected=%d CNT=%u\n",
                period, samples, last, expected, soc.tim1.reg[9]);
    assert(samples == 1u && last == expected);
    assert(mcu.base.pin_event_count <= SIM_MAX_PIN_EVENTS);
    py32_soc_destroy(&soc);
}

static void test_carrier_rejection(void)
{
    SimSpeaker speaker;
    sim_speaker_init(&speaker, "carrier-test", capture_quality, NULL, 44100u);
    for (unsigned period = 0u; period < 10000u; ++period) {
        speaker.base.ops->pin_changed(&speaker.base, 0u, SIM_LEVEL_HIGH);
        speaker.base.ops->pin_changed(&speaker.base, 1u, SIM_LEVEL_HIGH);
        speaker.base.ops->tick(&speaker.base, 96u, 48000000u);
        speaker.base.ops->pin_changed(&speaker.base, 0u, SIM_LEVEL_LOW);
        speaker.base.ops->tick(&speaker.base, 64u, 48000000u);
        speaker.base.ops->pin_changed(&speaker.base, 1u, SIM_LEVEL_LOW);
        speaker.base.ops->tick(&speaker.base, 96u, 48000000u);
    }
    assert(quality_samples > 2000u);
    assert(quality_peak_error < 150);
    /* 一阶矩积分的启动和群延迟，不能只靠整曲峰值证明滤波正确。 */
    sim_speaker_init(&speaker, "step-test", capture, NULL, 1u);
    samples = 0u;
    speaker.base.ops->pin_changed(&speaker.base, 1u, SIM_LEVEL_HIGH);
    speaker.base.ops->tick(&speaker.base, 4u, 4u);
    assert(samples == 1u && last == 16384);
    speaker.base.ops->pin_changed(&speaker.base, 1u, SIM_LEVEL_LOW);
    speaker.base.ops->tick(&speaker.base, 4u, 4u);
    assert(samples == 2u && last == 16384);
    speaker.base.ops->tick(&speaker.base, 4u, 4u);
    assert(samples == 3u && last == 0);
}

int main(void)
{
    test_pulses_inside_multiply(256u, 96u, 160u, 8192);
    test_pulses_inside_multiply(32u, 1u, 0u, -16384);
    test_carrier_rejection();
    puts("Audio reconstruction regression tests passed.");
    return 0;
}

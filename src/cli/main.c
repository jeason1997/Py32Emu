#define _POSIX_C_SOURCE 200809L

#include "py32emu/chips/chip.h"
#include "py32emu/chips/soc.h"
#include "py32emu/firmware/image.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

#if defined(PY32EMU_HAVE_PULSE)
#include <pulse/def.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#endif

typedef struct {
    Py32Soc *soc;
    FILE *wav;
    uint64_t wav_samples;
    bool wav_failed;
#if defined(PY32EMU_HAVE_PULSE)
    pa_simple *audio;
    int16_t audio_chunk[1024];
    unsigned audio_filled;
    uint64_t audio_failures;
#endif
} MediaCapture;

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static void install_stop_handlers(void)
{
#if defined(_WIN32)
    signal(SIGINT, request_stop);
#ifdef SIGTERM
    signal(SIGTERM, request_stop);
#endif
#else
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_stop;
    sigemptyset(&action.sa_mask);
    /* 不设置 SA_RESTART，让阻塞中的音频写入和 nanosleep 及时返回。 */
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
#ifdef SIGTERM
    sigaction(SIGTERM, &action, NULL);
#endif
#endif
}

static uint64_t monotonic_ns(void)
{
#if defined(_WIN32)
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (uint64_t)((double)counter.QuadPart * 1000000000.0 /
                      (double)frequency.QuadPart);
#else
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000u + (uint64_t)now.tv_nsec;
#endif
}

static void sleep_until_ns(uint64_t target)
{
    while (!stop_requested) {
        uint64_t now = monotonic_ns();
        uint64_t remaining;
        if (now >= target) return;
        remaining = target - now;
#if defined(_WIN32)
        Sleep((DWORD)((remaining + 999999u) / 1000000u));
#else
        {
            struct timespec delay;
            delay.tv_sec = (time_t)(remaining / 1000000000u);
            delay.tv_nsec = (long)(remaining % 1000000000u);
            nanosleep(&delay, NULL);
        }
#endif
    }
}

static bool wav_u16(FILE *file, uint16_t value)
{
    return fputc((int)(value & 0xFFu), file) != EOF &&
           fputc((int)(value >> 8), file) != EOF;
}

static bool wav_u32(FILE *file, uint32_t value)
{
    return wav_u16(file, (uint16_t)(value & 0xFFFFu)) &&
           wav_u16(file, (uint16_t)(value >> 16));
}

static bool wav_header(FILE *file, unsigned rate)
{
    static const char riff[] = "RIFF";
    static const char wave[] = "WAVEfmt ";
    static const char data[] = "data";
    return fwrite(riff, 1u, 4u, file) == 4u && wav_u32(file, 0u) &&
           fwrite(wave, 1u, 8u, file) == 8u && wav_u32(file, 16u) &&
           wav_u16(file, 1u) && wav_u16(file, 1u) &&
           wav_u32(file, rate) && wav_u32(file, rate * 2u) &&
           wav_u16(file, 2u) && wav_u16(file, 16u) &&
           fwrite(data, 1u, 4u, file) == 4u && wav_u32(file, 0u);
}

static bool wav_finish(FILE *file, uint64_t samples)
{
    uint32_t data_size = samples > UINT32_MAX / 2u
        ? UINT32_MAX & ~1u : (uint32_t)(samples * 2u);
    if (fseek(file, 4L, SEEK_SET) != 0 || !wav_u32(file, 36u + data_size) ||
        fseek(file, 40L, SEEK_SET) != 0 || !wav_u32(file, data_size) ||
        fflush(file) != 0) return false;
    return true;
}

#if defined(PY32EMU_HAVE_PULSE)
static void audio_flush(MediaCapture *capture)
{
    int error = 0;
    if (capture->audio == NULL || capture->audio_filled == 0u) return;
    if (pa_simple_write(capture->audio, capture->audio_chunk,
                        (size_t)capture->audio_filled * sizeof(int16_t),
                        &error) < 0) {
        ++capture->audio_failures;
        fprintf(stderr, "PulseAudio 写入失败: %s\n", pa_strerror(error));
        pa_simple_free(capture->audio);
        capture->audio = NULL;
    }
    capture->audio_filled = 0;
}

static bool audio_open(MediaCapture *capture, unsigned rate)
{
    pa_sample_spec spec;
    pa_buffer_attr attr;
    int error = 0;
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = rate;
    spec.channels = 1;
    /* 约 100 ms 目标缓冲：足够吸收宿主调度抖动，又不会让控制延迟过大。 */
    attr.maxlength = (uint32_t)-1;
    attr.tlength = rate * 2u / 10u;
    attr.prebuf = (uint32_t)-1;
    attr.minreq = (uint32_t)-1;
    attr.fragsize = (uint32_t)-1;
    capture->audio = pa_simple_new(NULL, "Py32Emu", PA_STREAM_PLAYBACK, NULL,
                                   "NSF speaker", &spec, NULL, &attr, &error);
    if (capture->audio == NULL)
        capture->audio = pa_simple_new(NULL, "Py32Emu", PA_STREAM_PLAYBACK,
                                       NULL, "NSF speaker", &spec, NULL, NULL,
                                       &error);
    if (capture->audio == NULL) {
        fprintf(stderr, "PulseAudio 打开失败: %s\n", pa_strerror(error));
        return false;
    }
    return true;
}

static void audio_close(MediaCapture *capture)
{
    int error = 0;
    audio_flush(capture);
    if (capture->audio != NULL) {
        /* Ctrl+C 时立即停止，不再等待声卡排空；正常结束才 drain 尾音。 */
        if (!stop_requested) (void)pa_simple_drain(capture->audio, &error);
        pa_simple_free(capture->audio);
        capture->audio = NULL;
    }
}
#endif

static void media_capture_compare(void *context, unsigned channel,
                                  uint32_t value)
{
    MediaCapture *capture = context;
    int16_t pcm;
    if (capture == NULL || channel != 4u) return;
    /* 只在固件启动 TIM16 采样中断后记录；初始化阶段的中点写入忽略。 */
    if ((capture->soc->tim1.reg[0] & 1u) == 0u ||
        (capture->soc->tim16.reg[0] & 1u) == 0u) return;
    if (value > 255u) value = 255u;
    pcm = (int16_t)(((int32_t)value - 128) * 256);
    if (capture->wav != NULL && !capture->wav_failed) {
        if (fwrite(&pcm, sizeof(pcm), 1u, capture->wav) != 1u)
            capture->wav_failed = true;
        else ++capture->wav_samples;
    }
#if defined(PY32EMU_HAVE_PULSE)
    if (capture->audio != NULL) {
        capture->audio_chunk[capture->audio_filled++] = pcm;
        if (capture->audio_filled ==
            sizeof(capture->audio_chunk) / sizeof(capture->audio_chunk[0]))
            audio_flush(capture);
    }
#endif
}

int main(int argc, char **argv)
{
    const Py32ChipDescription *chip = py32f002ax5_description();
    Py32FirmwareImage image;
    Py32Soc soc;
    uint64_t limit = 1000000u;
    uint64_t steps = 0;
    bool trace = false;
    bool pulse = false;
    unsigned pulse_port = 0u, pulse_pin = 0u;
    uint64_t pulse_at = 10000u;
    bool analog = false;
    unsigned analog_port = 0u, analog_pin = 0u, analog_mv = 0u;
    uint64_t analog_at = 10000u;
    const char *wav_path = NULL;
    unsigned wav_rate = 22050u;
    unsigned audio_rate = 22050u;
    uint32_t clock_hz = 48000000u;
    bool audio = false;
    bool realtime = false;
    bool limit_set = false;
    int i;
    char error[160];

    if (argc < 2) {
        fprintf(stderr, "用法: %s firmware [--chip 型号] [--steps N] [--trace] "
                        "[--wav 文件] [--wav-rate Hz] [--audio] "
                        "[--audio-rate Hz] [--realtime] [--clock-hz Hz]\n",
                argv[0]);
        return 2;
    }
    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--trace") == 0) trace = true;
        else if (strcmp(argv[i], "--chip") == 0 && i + 1 < argc) {
            chip = py32_chip_by_name(argv[++i]);
            if (chip == NULL) {
                fprintf(stderr, "未知芯片型号: %s\n", argv[i]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc) {
            limit = strtoull(argv[++i], NULL, 0);
            limit_set = true;
        }
        else if (strcmp(argv[i], "--pulse") == 0 && i + 1 < argc) {
            const char *pin = argv[++i];
            char *end;
            if (strlen(pin) < 3u || pin[0] != 'P' ||
                (pin[1] != 'A' && pin[1] != 'B' && pin[1] != 'F')) {
                fprintf(stderr, "无效 GPIO 引脚: %s\n", pin);
                return 2;
            }
            pulse_port = pin[1] == 'A' ? 0u : pin[1] == 'B' ? 1u : 2u;
            pulse_pin = (unsigned)strtoul(pin + 2, &end, 10);
            if (*end != '\0' || pulse_pin >= 16u) {
                fprintf(stderr, "无效 GPIO 引脚: %s\n", pin);
                return 2;
            }
            pulse = true;
        }
        else if (strcmp(argv[i], "--pulse-at") == 0 && i + 1 < argc)
            pulse_at = strtoull(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--analog") == 0 && i + 1 < argc) {
            const char *argument = argv[++i];
            char *end;
            unsigned long pin, millivolts;
            if (strlen(argument) < 5u || argument[0] != 'P' ||
                (argument[1] != 'A' && argument[1] != 'B' &&
                 argument[1] != 'F')) {
                fprintf(stderr, "无效模拟输入: %s\n", argument);
                return 2;
            }
            analog_port = argument[1] == 'A' ? 0u
                          : argument[1] == 'B' ? 1u : 2u;
            pin = strtoul(argument + 2, &end, 10);
            if (*end != '=' || pin >= 16u) {
                fprintf(stderr, "无效模拟输入: %s\n", argument);
                return 2;
            }
            millivolts = strtoul(end + 1, &end, 10);
            if (*end != '\0' || millivolts > 5000u) {
                fprintf(stderr, "无效模拟电压: %s\n", argument);
                return 2;
            }
            analog_pin = (unsigned)pin;
            analog_mv = (unsigned)millivolts;
            analog = true;
        }
        else if (strcmp(argv[i], "--analog-at") == 0 && i + 1 < argc)
            analog_at = strtoull(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--wav") == 0 && i + 1 < argc)
            wav_path = argv[++i];
        else if (strcmp(argv[i], "--wav-rate") == 0 && i + 1 < argc) {
            char *end;
            unsigned long rate = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || rate < 1000u || rate > 192000u) {
                fprintf(stderr, "无效 WAV 采样率: %s\n", argv[i]);
                return 2;
            }
            wav_rate = (unsigned)rate;
        }
        else if (strcmp(argv[i], "--audio") == 0) {
            audio = true;
            realtime = true;
        }
        else if (strcmp(argv[i], "--realtime") == 0) realtime = true;
        else if (strcmp(argv[i], "--audio-rate") == 0 && i + 1 < argc) {
            char *end;
            unsigned long rate = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || rate < 1000u || rate > 192000u) {
                fprintf(stderr, "无效音频采样率: %s\n", argv[i]);
                return 2;
            }
            audio_rate = (unsigned)rate;
        }
        else if (strcmp(argv[i], "--clock-hz") == 0 && i + 1 < argc) {
            char *end;
            unsigned long hz = strtoul(argv[++i], &end, 10);
            if (*end != '\0' || hz < 1000u || hz > 200000000u) {
                fprintf(stderr, "无效系统时钟: %s\n", argv[i]);
                return 2;
            }
            clock_hz = (uint32_t)hz;
        }
        else {
            fprintf(stderr, "未知参数: %s\n", argv[i]);
            return 2;
        }
    }
#if !defined(PY32EMU_HAVE_PULSE)
    (void)audio_rate;
    if (audio) {
        fprintf(stderr, "当前构建未启用 PulseAudio，不能使用 --audio；"
                        "请安装 libpulse-dev 后重新编译。\n");
        return 2;
    }
#endif
    if (!limit_set && (audio || realtime)) limit = UINT64_MAX;
    install_stop_handlers();
    py32_firmware_image_init(&image);
    py32_soc_init(&soc);
    {
        size_t path_length = strlen(argv[1]);
        bool is_hex = path_length >= 4u &&
            strcmp(argv[1] + path_length - 4u, ".hex") == 0;
        bool is_elf = path_length >= 4u &&
            strcmp(argv[1] + path_length - 4u, ".elf") == 0;
        bool loaded = is_elf
            ? py32_firmware_load_elf(&image, argv[1], chip->flash_base,
                                     chip->flash_size, error, sizeof(error))
            : is_hex
                ? py32_firmware_load_hex(&image, argv[1], chip->flash_base,
                                         chip->flash_size, error, sizeof(error))
                : py32_firmware_load_bin(&image, argv[1], chip->flash_base,
                                         chip->flash_size, error, sizeof(error));
        if (!loaded) {
        fprintf(stderr, "加载失败: %s\n", error);
        return 1;
        }
    }
    printf("已加载 %zu 字节到 %s Flash @ 0x%08X\n", image.size,
           chip->name, chip->flash_base);
    if (!py32_soc_configure(&soc, chip, &image, error, sizeof(error)) ||
        !py32_soc_reset(&soc, error, sizeof(error))) {
        fprintf(stderr, "启动失败: %s\n", error);
        py32_soc_destroy(&soc);
        py32_firmware_image_free(&image);
        return 1;
    }
    /* --pulse 表示确定的高到低脉冲，不依赖固件是否配置内部上拉。 */
    if (pulse)
        py32_soc_set_gpio_input(&soc, pulse_port, pulse_pin, true, true);
    MediaCapture capture;
    memset(&capture, 0, sizeof(capture));
    capture.soc = &soc;
    bool wav_error = false;
    bool audio_error = false;
    uint64_t real_start_ns = 0u;
    uint64_t real_start_cycles = 0u;
    if (wav_path != NULL) {
        capture.wav = fopen(wav_path, "wb");
        if (capture.wav == NULL || !wav_header(capture.wav, wav_rate)) {
            fprintf(stderr, "无法创建 WAV 文件: %s\n", wav_path);
            if (capture.wav != NULL) fclose(capture.wav);
            py32_soc_destroy(&soc);
            py32_firmware_image_free(&image);
            return 1;
        }
    }
#if defined(PY32EMU_HAVE_PULSE)
    if (audio) {
        if (!audio_open(&capture, audio_rate)) {
            if (capture.wav != NULL) fclose(capture.wav);
            py32_soc_destroy(&soc);
            py32_firmware_image_free(&image);
            return 1;
        }
        printf("AUDIO: PulseAudio %u Hz，Ctrl+C 停止\n", audio_rate);
        fflush(stdout);
    }
#endif
    if (wav_path != NULL || audio) {
        soc.tim1.compare_callback = media_capture_compare;
        soc.tim1.compare_context = &capture;
    }
    {
        uint64_t start_ns = realtime ? monotonic_ns() : 0u;
        uint64_t start_cycles = soc.cpu.cycles;
        uint64_t next_slice = start_cycles + clock_hz / 1000u;
        real_start_ns = start_ns;
        real_start_cycles = start_cycles;
        while (!soc.cpu.stopped && steps < limit && !stop_requested) {
            if (pulse && steps == pulse_at)
                py32_soc_set_gpio_input(&soc, pulse_port, pulse_pin,
                                        true, false);
            if (analog && steps == analog_at)
                py32_soc_set_analog_input(&soc, analog_port, analog_pin,
                                          (uint16_t)analog_mv);
            {
                CortexM0StepResult result = py32_soc_step(&soc);
                if (trace) {
                    const Py32FirmwareSymbol *symbol =
                        py32_firmware_find_symbol(&image, result.executed_pc);
                    printf("%08X  %04X  r0=%08X sp=%08X",
                           result.executed_pc, result.instruction,
                           soc.cpu.r[0], soc.cpu.r[CORTEX_M0_SP]);
                    if (symbol != NULL)
                        printf("  <%s+0x%X>", symbol->name,
                               result.executed_pc - symbol->address);
                    putchar('\n');
                }
            }
            ++steps;
            if (realtime && soc.cpu.cycles >= next_slice) {
                uint64_t elapsed = soc.cpu.cycles - start_cycles;
                uint64_t seconds = elapsed / clock_hz;
                uint64_t remainder = elapsed % clock_hz;
                uint64_t target = start_ns + seconds * 1000000000u +
                                  remainder * 1000000000u / clock_hz;
                sleep_until_ns(target);
                next_slice = soc.cpu.cycles + clock_hz / 1000u;
            }
        }
    }
    soc.tim1.compare_callback = NULL;
#if defined(PY32EMU_HAVE_PULSE)
    if (audio) {
        audio_close(&capture);
        if (capture.audio_failures != 0u) audio_error = true;
        printf("AUDIO: %llu 个写入失败\n",
               (unsigned long long)capture.audio_failures);
    }
#endif
    if (realtime) {
        double wall = (double)(monotonic_ns() - real_start_ns) / 1e9;
        double emulated = (double)(soc.cpu.cycles - real_start_cycles) /
                          (double)clock_hz;
        printf("CLOCK: 模拟 %.3f 秒 / 墙钟 %.3f 秒 = %.2f 倍实时\n",
               emulated, wall, wall > 0.0 ? emulated / wall : 0.0);
    }
    printf("执行 %llu 条，%llu 周期，PC=0x%08X，状态=%s\n",
           (unsigned long long)steps,
           (unsigned long long)soc.cpu.cycles,
           soc.cpu.r[CORTEX_M0_PC],
           soc.cpu.stopped ? cortex_m0_stop_reason_name(soc.cpu.stop_reason)
                           : stop_requested ? "interrupted" : "step-limit");
    if (wav_path != NULL) {
        bool ok = !capture.wav_failed &&
                  wav_finish(capture.wav, capture.wav_samples);
        if (fclose(capture.wav) != 0) ok = false;
        if (ok)
            printf("WAV: %s, %llu 样本, %.2f 秒 @ %u Hz\n", wav_path,
                   (unsigned long long)capture.wav_samples,
                   (double)capture.wav_samples / (double)wav_rate, wav_rate);
        else {
            fprintf(stderr, "WAV 写入失败: %s\n", wav_path);
            wav_error = true;
        }
    }
#if defined(PY32EMU_HAVE_PULSE)
    if (audio) {
        audio_close(&capture);
        if (capture.audio_failures != 0u) audio_error = true;
        printf("AUDIO: %llu 个写入失败\n",
               (unsigned long long)capture.audio_failures);
    }
#endif
    if (soc.bus.faulted)
        printf("BUS: 故障地址=0x%08X\n", soc.bus.fault_address);
    printf("GPIOA: MODER=0x%08X ODR=0x%04X\n",
           soc.gpioa.moder, soc.gpioa.odr);
    if (pulse)
        printf("EXTI: RTSR=0x%X FTSR=0x%X IMR=0x%X PR=0x%X\n",
               soc.exti.rtsr, soc.exti.ftsr, soc.exti.imr, soc.exti.pr);
    if (soc.reset_count != 0u)
        printf("RESET: IWDG=%llu CSR=0x%08X\n",
               (unsigned long long)soc.reset_count, soc.rcc.csr);
    if (soc.flash_controller.erase_count != 0u ||
        soc.flash_controller.program_word_count != 0u) {
        uint32_t flash_word = 0u;
        py32_flash_memory_read(&soc.flash_controller,
                               0x2000u, 4u, &flash_word);
        printf("FLASH: erase=%llu program-words=%llu last=0x%08X "
               "@0x08002000=0x%08X\n",
               (unsigned long long)soc.flash_controller.erase_count,
               (unsigned long long)soc.flash_controller.program_word_count,
               soc.flash_controller.last_address, flash_word);
    }
    if ((soc.rcc.apbenr2 & (1u << 11)) != 0u)
        printf("TIM1: CNT=%u PSC=%u ARR=%u CCR3=%u CCR4=%u SR=0x%X\n",
               (unsigned)soc.tim1.reg[9], (unsigned)soc.tim1.reg[10],
               (unsigned)soc.tim1.reg[11], (unsigned)soc.tim1.reg[15],
               (unsigned)soc.tim1.reg[16], (unsigned)soc.tim1.reg[4]);
    if ((soc.rcc.apbenr2 & (1u << 20)) != 0u)
        printf("ADC1: DR=%u CHSELR=0x%X conversions=%llu\n",
               (unsigned)soc.adc1.dr, (unsigned)soc.adc1.chselr,
               (unsigned long long)soc.adc1.conversion_count);
    if (py32_chip_has_peripheral(chip, PY32_PERIPHERAL_LPTIM1) &&
        (soc.rcc.apbenr1 & (1u << 31)) != 0u)
        printf("LPTIM1: CNT=%u ARR=%u ISR=0x%X matches=%llu\n",
               (unsigned)soc.lptim1.cnt, (unsigned)soc.lptim1.arr,
               (unsigned)soc.lptim1.isr,
               (unsigned long long)soc.lptim1.match_count);
    if (py32_chip_has_peripheral(chip, PY32_PERIPHERAL_COMP12) &&
        (soc.rcc.apbenr2 & ((1u << 21) | (1u << 22))) != 0u)
        printf("COMP: CSR1=0x%08X CSR2=0x%08X transitions=%llu/%llu\n",
               soc.comp12.csr[0], soc.comp12.csr[1],
               (unsigned long long)soc.comp12.transition_count[0],
               (unsigned long long)soc.comp12.transition_count[1]);
    if (py32_chip_has_peripheral(chip, PY32_PERIPHERAL_CRC) &&
        (soc.rcc.ahbenr & (1u << 12)) != 0u)
        printf("CRC: DR=0x%08X IDR=0x%02X\n",
               soc.crc.dr, soc.crc.idr);
    if (soc.usart1.tx_count > 0u) {
        size_t n;
        printf("USART1 TX (%zu):", soc.usart1.tx_count);
        for (n = 0; n < soc.usart1.tx_count; ++n)
            printf(" %02X", soc.usart1.tx[n]);
        putchar('\n');
    }
    if (soc.spi1.tx_count > 0u) {
        size_t n;
        printf("SPI1 TX (%zu):", soc.spi1.tx_count);
        for (n = 0; n < soc.spi1.tx_count; ++n)
            printf(" %02X", soc.spi1.tx[n]);
        putchar('\n');
    }
    if (soc.i2c1.tx_count > 0u) {
        printf("I2C1 TX (%zu):", soc.i2c1.tx_count);
        for (i = 0; i < (int)soc.i2c1.tx_count; ++i)
            printf(" %02X", soc.i2c1.tx[i]);
        putchar('\n');
    }
    if (soc.i2c1.rx_count > 0u) {
        printf("I2C1 RX (%zu):", soc.i2c1.rx_count);
        for (i = 0; i < (int)soc.i2c1.rx_count; ++i)
            printf(" %02X", soc.i2c1.rx[i]);
        putchar('\n');
    }
    py32_soc_destroy(&soc);
    py32_firmware_image_free(&image);
    return (wav_error || audio_error) ? 1 : 0;
}

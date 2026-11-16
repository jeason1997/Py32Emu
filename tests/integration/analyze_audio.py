"""对齐模拟器 PCM 与声卡回录，报告延迟变化、局部相关度和削波。"""

import argparse
import json
from pathlib import Path
import wave

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def read_wav(path):
    with wave.open(str(path), "rb") as source:
        if source.getsampwidth() != 2:
            raise ValueError("需要 16 位 PCM WAV")
        rate = source.getframerate()
        samples = np.frombuffer(source.readframes(source.getnframes()), "<i2")
        return rate, samples.reshape(-1, source.getnchannels()).mean(axis=1)


def correlation(signal, template):
    size = 1 << (len(signal) + len(template) - 1).bit_length()
    values = np.fft.irfft(np.fft.rfft(signal, size)
                          * np.conj(np.fft.rfft(template, size)), size)
    return values[:len(signal) - len(template) + 1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("capture", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    rate, reference = read_wav(args.reference)
    capture_rate, capture = read_wav(args.capture)
    if rate != capture_rate:
        raise ValueError("请先将两份音频转换为同一采样率")
    args.output.mkdir(parents=True, exist_ok=True)
    # 从首个有声窗口估计初始偏移，再逐窗追踪，避免断音后全局对齐失效。
    width = rate // 20
    times, delays, scores = [], [], []
    lag = 0
    for start in range(0, len(reference) - width, width):
        template = reference[start:start + width]
        energy = np.dot(template, template)
        if energy < width * 30 ** 2:
            continue
        first = max(0, start + lag - rate // 4)
        last = min(len(capture), start + lag + width + rate // 4)
        if not times:
            first, last = 0, len(capture)
        segment = capture[first:last]
        if len(segment) < width:
            break
        corr = correlation(segment, template)
        squared = np.concatenate(([0.0], np.cumsum(segment * segment)))
        energies = squared[width:] - squared[:-width]
        normalized = corr / np.sqrt(np.maximum(energies * energy, 1.0))
        peak = int(np.argmax(normalized))
        score = float(normalized[peak])
        if score >= 0.7:
            lag = first + peak - start
        times.append(start / rate)
        delays.append((first + peak - start) / rate)
        scores.append(score)
    if not times:
        raise ValueError("参考音频没有足够的有效信号")
    delays = np.array(delays)
    scores = np.array(scores)
    reliable = scores >= 0.9
    report = {
        "rate_hz": rate,
        "reference_seconds": len(reference) / rate,
        "capture_seconds": len(capture) / rate,
        "reference_peak": float(np.max(np.abs(reference))),
        "capture_peak": float(np.max(np.abs(capture))),
        "reference_clipped_samples": int(np.sum(np.abs(reference) >= 32767)),
        "capture_clipped_samples": int(np.sum(np.abs(capture) >= 32767)),
        "analyzed_windows": len(scores),
        "correlation_median": float(np.median(scores)),
        "windows_below_0_9": int(np.sum(~reliable)),
        "delay_range_ms": float(np.ptp(delays[reliable]) * 1000)
        if np.any(reliable) else None,
        "note": "低相关窗口也可能来自重采样、系统音效或重复旋律误匹配，需结合波形判断。",
    }
    (args.output / "report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    figure, axes = plt.subplots(3, 1, figsize=(12, 8), constrained_layout=True)
    axes[0].plot(np.arange(0, len(reference), 50) / rate,
                 reference[::50] / 32768, label="Emulator PCM", linewidth=0.6)
    initial_delay = float(delays[0])
    axes[0].plot(np.arange(0, len(capture), 50) / rate - initial_delay,
                 capture[::50] / 32768, label="Playback loopback", alpha=0.7,
                 linewidth=0.6)
    axes[0].set_ylabel("Amplitude")
    axes[0].legend()
    axes[1].plot(times, delays * 1000, ".-", markersize=2)
    axes[1].set_ylabel("Local delay (ms)")
    axes[2].plot(times, scores, ".-", markersize=2)
    axes[2].axhline(0.9, color="red", linestyle="--")
    axes[2].set_ylabel("Local correlation")
    axes[2].set_xlabel("Reference time (s)")
    for axis in axes:
        axis.grid(alpha=0.2)
    figure.savefig(args.output / "waveform-comparison.png", dpi=150)


if __name__ == "__main__":
    main()

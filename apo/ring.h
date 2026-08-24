#pragma once
// ─── EchoBox APO — общий ring-буфер ──────────────────────────────────────────
// Один и тот же заголовок компилируется в двух местах:
//   1. В APO DLL (MSVC) — читатель. APO создаёт mapping, т.к. audiodg работает
//      как служба и имеет право создавать объекты в Global\ namespace.
//   2. В EchoBox (MinGW) — писатель. Плеер только открывает готовый mapping.
// Поэтому здесь чистый C-layout: никаких классов, только POD + volatile.

#include <stdint.h>

#pragma pack(push, 8)
struct EchoBoxRing {
    uint32_t          magic;          // должен быть kEchoBoxRingMagic
    uint32_t          version;        // 2
    uint32_t          sampleRate;     // частота записываемых плеером данных
    uint32_t          channels;       // всегда 2 (float32 interleaved stereo)
    volatile uint32_t writePos;       // индекс кадра (по модулю capacityFrames)
    volatile uint32_t readPos;        // индекс кадра (по модулю capacityFrames)
    uint32_t          capacityFrames; // ёмкость в кадрах
    volatile uint32_t playerAlive;    // плеер обновляет каждый tick (heartbeat)
    volatile uint32_t blockVoice;     // 1 = глушить реальный микрофон (только музыка)
    volatile float    musicGain;      // множитель громкости музыки (1.0 = как есть)
    volatile uint32_t noiseGate;      // 1 = шумоподавление голоса (гейт + HPF)
    volatile float    gateThresh;     // порог открытия гейта (линейная амплитуда)
    float             data[1];        // дальше идут capacityFrames*2 float'ов
};
#pragma pack(pop)

static const wchar_t  kEchoBoxRingName[]   = L"Global\\EchoBoxApoRing";
static const uint32_t kEchoBoxRingMagic    = 0x45424150u; // 'EBAP'
static const uint32_t kEchoBoxRingVersion  = 2;
static const uint32_t kRingCapacityFrames  = 48000;       // 1 секунда при 48 кГц

inline uint32_t echoBoxRingBytes()
{
    return uint32_t(sizeof(EchoBoxRing)) +
           (kRingCapacityFrames * 2 - 1) * uint32_t(sizeof(float));
}

#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace automech
{
enum ScaleIndex
{
    scaleChromatic = 0, scaleMajor, scaleMinor, scaleDorian, scalePhrygian, scaleLydian,
    scaleMixolydian, scalePentaMajor, scalePentaMinor, scaleBlues, scaleHarmonicMinor, scaleCustom,
    numScales
};

// Pitch classes are bits 0..11 of a 12-bit mask (bit 0 = C).
struct ScaleQuantizer
{
    static uint16_t maskFrom (std::initializer_list<int> intervals) noexcept
    {
        uint16_t m = 0;
        for (auto i : intervals) m = (uint16_t) (m | (1u << (unsigned) i));
        return m;
    }

    // Mask relative to the root (bit 0 = root).
    static uint16_t relativeMask (int scale) noexcept
    {
        switch (scale)
        {
            case scaleMajor:         return maskFrom ({ 0, 2, 4, 5, 7, 9, 11 });
            case scaleMinor:         return maskFrom ({ 0, 2, 3, 5, 7, 8, 10 });
            case scaleDorian:        return maskFrom ({ 0, 2, 3, 5, 7, 9, 10 });
            case scalePhrygian:      return maskFrom ({ 0, 1, 3, 5, 7, 8, 10 });
            case scaleLydian:        return maskFrom ({ 0, 2, 4, 6, 7, 9, 11 });
            case scaleMixolydian:    return maskFrom ({ 0, 2, 4, 5, 7, 9, 10 });
            case scalePentaMajor:    return maskFrom ({ 0, 2, 4, 7, 9 });
            case scalePentaMinor:    return maskFrom ({ 0, 3, 5, 7, 10 });
            case scaleBlues:         return maskFrom ({ 0, 3, 5, 6, 7, 10 });
            case scaleHarmonicMinor: return maskFrom ({ 0, 2, 3, 5, 7, 8, 11 });
            default:                 return 0x0FFF;
        }
    }

    // Mask in absolute pitch classes. customMask is already absolute.
    static uint16_t absoluteMask (int key, int scale, uint16_t customMask) noexcept
    {
        if (scale == scaleCustom)
            return (uint16_t) (customMask & 0x0FFF);

        const uint16_t rel = relativeMask (scale);
        uint16_t out = 0;
        for (int i = 0; i < 12; ++i)
            if (rel & (1u << (unsigned) i))
                out = (uint16_t) (out | (1u << (unsigned) ((i + key) % 12)));
        return out;
    }

    static bool contains (uint16_t mask, int note) noexcept
    {
        return (mask >> (unsigned) (((note % 12) + 12) % 12)) & 1u;
    }

    // Nearest enabled note to `midi`, with hysteresis around the current note.
    static float nearest (float midi, uint16_t mask, float currentNote, float hysteresis) noexcept
    {
        if (mask == 0)
            return std::round (midi);

        const int base = (int) std::floor (midi);
        float best = (float) base;
        float bestDist = 1.0e9f;

        for (int n = base - 12; n <= base + 12; ++n)
        {
            if (! contains (mask, n))
                continue;
            const float d = std::fabs (midi - (float) n);
            if (d < bestDist) { bestDist = d; best = (float) n; }
        }

        if (currentNote >= 0.0f && contains (mask, (int) currentNote)
            && std::fabs (midi - currentNote) <= bestDist + hysteresis)
            return currentNote;

        return best;
    }

    // The note `steps` enabled notes above (or below) `note`.
    static int stepFrom (int note, int steps, uint16_t mask) noexcept
    {
        if (steps == 0) return note;
        if (mask == 0)  mask = 0x0FFF;

        const int dir = steps > 0 ? 1 : -1;
        int remaining = steps > 0 ? steps : -steps;
        int n = note;

        for (int guard = 0; guard < 96 && remaining > 0; ++guard)
        {
            n += dir;
            if (contains (mask, n)) --remaining;
        }
        return n;
    }

    // Pitch class `pc` placed in the octave closest to `reference`.
    static float foldNear (int pc, float reference) noexcept
    {
        return (float) pc + 12.0f * std::round ((reference - (float) pc) / 12.0f);
    }
};
} // namespace automech

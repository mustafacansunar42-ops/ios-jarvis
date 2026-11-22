#pragma once

#include <juce_core/juce_core.h>
#include <array>

namespace frier
{
/** Single-producer (audio thread) / single-consumer (GUI thread) lock-free FIFO
    carrying a mono dry stream and a mono fried stream, sample-aligned. */
class VisualizerFifo
{
public:
    static constexpr int capacity = 1 << 15;

    /** Audio thread. Drops samples when the GUI is not draining the FIFO. */
    void push (const float* dry, const float* wet, int numSamples) noexcept
    {
        const auto scope = fifo.write (numSamples);
        copyIn (dry, wet, scope.startIndex1, scope.blockSize1, 0);
        copyIn (dry, wet, scope.startIndex2, scope.blockSize2, scope.blockSize1);
    }

    /** GUI thread. Returns the number of samples read. */
    int pull (float* dry, float* wet, int maxSamples) noexcept
    {
        const auto scope = fifo.read (maxSamples);
        copyOut (dry, wet, scope.startIndex1, scope.blockSize1, 0);
        copyOut (dry, wet, scope.startIndex2, scope.blockSize2, scope.blockSize1);
        return scope.blockSize1 + scope.blockSize2;
    }

private:
    void copyIn (const float* dry, const float* wet, int start, int size, int offset) noexcept
    {
        for (int i = 0; i < size; ++i)
        {
            dryBuffer[(size_t) (start + i)] = dry[offset + i];
            wetBuffer[(size_t) (start + i)] = wet[offset + i];
        }
    }

    void copyOut (float* dry, float* wet, int start, int size, int offset) const noexcept
    {
        for (int i = 0; i < size; ++i)
        {
            dry[offset + i] = dryBuffer[(size_t) (start + i)];
            wet[offset + i] = wetBuffer[(size_t) (start + i)];
        }
    }

    juce::AbstractFifo fifo { capacity };
    std::array<float, capacity> dryBuffer {}, wetBuffer {};
};
} // namespace frier

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cstdint>

// Observation only, owned by KWin's compositor thread. No transport decisions.
// Revisions describe completed paint calls, NOT GPU completion or physical scanout.
struct AnlandFrameCounters {
    struct Sample {
        bool painted;
        bool mismatch;
        bool regression;
        std::uint64_t revision;
        std::uint64_t previousRevision;
    };

    std::uint64_t renders = 0, presents = 0, accepted = 0;
    std::uint64_t unpainted = 0, unpaintedWithDebt = 0;
    std::uint64_t mismatches = 0, regressions = 0, unfenced = 0;
    int renderedSlot = -1;

    void resetSession()
    {
        slots.fill(0);
        sceneRevision = shownRevision = 0;
        paintedSincePresent = false;
        renderedSlot = -1;
    }

    void render(int slot, bool paintedRegion, bool changedScene)
    {
        ++renders;
        paintedSincePresent = true;
        renderedSlot = slot;
        if (changedScene) {
            ++sceneRevision;
        }
        if (paintedRegion && valid(slot)) {
            slots[slot] = sceneRevision;
        }
    }

    Sample present(int slot, bool owesDamage, bool hasFence, bool handedOff)
    {
        const auto revision = valid(slot) ? slots[slot] : 0;
        const Sample result{paintedSincePresent,
                            paintedSincePresent && slot != renderedSlot,
                            handedOff && valid(slot) && revision < shownRevision,
                            revision, shownRevision};
        ++presents;
        accepted += handedOff;
        unpainted += !result.painted;
        unpaintedWithDebt += !result.painted && owesDamage;
        mismatches += result.mismatch;
        regressions += result.regression;
        unfenced += !hasFence;
        if (handedOff && valid(slot) && revision > shownRevision) {
            shownRevision = revision;
        }
        paintedSincePresent = false;
        return result;
    }

private:
    static bool valid(int slot) { return slot >= 0 && slot < 8; }
    std::array<std::uint64_t, 8> slots{};
    std::uint64_t sceneRevision = 0, shownRevision = 0;
    bool paintedSincePresent = false;
};

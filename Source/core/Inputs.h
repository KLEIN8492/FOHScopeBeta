#pragma once
#include "core/PortRef.h"
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <vector>

namespace fs {

// Trace colours: each new input steps the hue by the golden angle (≈ 137.5°), so colours look
// random but never land next to an earlier one (≥ 20° apart for the first 13). Inputs 14+ reuse
// those hues at a lower brightness.
struct Palette
{
    static constexpr int kDistinctHues = 13;
    static float hueDegrees(int index) { return std::fmod(30.0f + 137.50776f * float(index), 360.0f); }
    static juce::Colour colour(int index)
    {
        const float brightness = (index / kDistinctHues) % 2 == 0 ? 0.98f : 0.72f;
        return juce::Colour::fromHSV(hueDegrees(index) / 360.0f, 0.72f, brightness, 1.0f);
    }
};

struct Input
{
    int id = 0;
    juce::String name;
    PortRef port;         // interface + 0-based channel; empty device = not patched
    juce::Colour colour;
    bool muted = false;   // hidden and not analysed
    bool splMic = false;  // the calibrated measurement mic (SPL meter + logging)
};

struct InputList
{
    static constexpr int kMax = 16; // and 0 is allowed: the routing page starts empty

    std::vector<Input> inputs;
    int focusedId = 0;
    int deltaA = -1, deltaB = -1; // input ids, -1 = off
    int nextColour = 0;

    const Input* find(int id) const
    {
        auto it = std::find_if(inputs.begin(), inputs.end(), [id](auto& i) { return i.id == id; });
        return it == inputs.end() ? nullptr : &*it;
    }
    Input* find(int id) { return const_cast<Input*>(std::as_const(*this).find(id)); }
    const Input* splMic() const
    {
        auto it = std::find_if(inputs.begin(), inputs.end(), [](auto& i) { return i.splMic && !i.muted; });
        return it == inputs.end() ? nullptr : &*it;
    }

    // Adds an input on `port` with the next palette colour. Returns its id, or -1 when full.
    // The first input becomes the SPL mic and gets the focus.
    int add(const PortRef& port)
    {
        if (int(inputs.size()) >= kMax) return -1;
        int id = 1;
        for (auto& i : inputs) id = std::max(id, i.id + 1);
        inputs.push_back({ id, "Input " + juce::String(id), port, Palette::colour(nextColour++), false, inputs.empty() });
        if (inputs.size() == 1) focusedId = id;
        return id;
    }

    // Next channel of `device` that no input uses yet (0 if all are used or none exist).
    int nextFreeChannel(const juce::String& device, int channels) const
    {
        for (int ch = 0; ch < channels; ++ch)
            if (std::none_of(inputs.begin(), inputs.end(), [&](auto& i) { return i.port == PortRef { device, ch }; })) return ch;
        return 0;
    }

    bool remove(int id)
    {
        const auto before = inputs.size();
        inputs.erase(std::remove_if(inputs.begin(), inputs.end(), [id](auto& i) { return i.id == id; }), inputs.end());
        if (inputs.size() == before) return false;
        if (focusedId == id) focusedId = inputs.empty() ? 0 : inputs.front().id;
        if (deltaA == id) deltaA = -1;
        if (deltaB == id) deltaB = -1;
        return true;
    }

    void setSplMic(int id)
    {
        for (auto& i : inputs) i.splMic = i.id == id;
    }

    bool operator==(const InputList& o) const
    {
        auto same = [](const Input& a, const Input& b) {
            return a.id == b.id && a.name == b.name && a.port == b.port && a.colour == b.colour && a.muted == b.muted && a.splMic == b.splMic;
        };
        return focusedId == o.focusedId && deltaA == o.deltaA && deltaB == o.deltaB && nextColour == o.nextColour
               && std::equal(inputs.begin(), inputs.end(), o.inputs.begin(), o.inputs.end(), same);
    }
};

} // namespace fs

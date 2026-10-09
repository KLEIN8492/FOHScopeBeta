#pragma once
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
    int channel = 0;      // 0-based device input channel
    juce::Colour colour;
    bool muted = false;   // hidden and not analysed
    bool splMic = false;  // the calibrated measurement mic (SPL meter + logging)
};

struct InputList
{
    static constexpr int kMax = 16;

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

    // Adds an input on the next unused channel with the next palette colour. Returns its id or -1.
    int add(int deviceChannels)
    {
        if (int(inputs.size()) >= kMax) return -1;
        int id = 1;
        for (auto& i : inputs) id = std::max(id, i.id + 1);
        int ch = 0;
        while (ch < std::max(deviceChannels, 1) - 1 && std::any_of(inputs.begin(), inputs.end(), [ch](auto& i) { return i.channel == ch; })) ++ch;
        inputs.push_back({ id, "Input " + juce::String(ch + 1), ch, Palette::colour(nextColour++), false, inputs.empty() });
        if (inputs.size() == 1) focusedId = id;
        return id;
    }

    // Never removes the last input.
    bool remove(int id)
    {
        if (inputs.size() <= 1) return false;
        const auto before = inputs.size();
        inputs.erase(std::remove_if(inputs.begin(), inputs.end(), [id](auto& i) { return i.id == id; }), inputs.end());
        if (inputs.size() == before) return false;
        if (focusedId == id) focusedId = inputs.front().id;
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
            return a.id == b.id && a.name == b.name && a.channel == b.channel && a.colour == b.colour && a.muted == b.muted && a.splMic == b.splMic;
        };
        return focusedId == o.focusedId && deltaA == o.deltaA && deltaB == o.deltaB && nextColour == o.nextColour
               && std::equal(inputs.begin(), inputs.end(), o.inputs.begin(), o.inputs.end(), same);
    }
};

} // namespace fs

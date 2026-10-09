#pragma once
#include <juce_core/juce_core.h>
#include <optional>

namespace fs {

// One channel of one audio interface, e.g. { "Dante Virtual Soundcard", 3 } (same as LiveRack).
// Channels are 0-based; the UI shows them 1-based. An empty device means "not patched".
struct PortRef
{
    juce::String device;
    int channel = 0;

    bool valid() const { return device.isNotEmpty() && channel >= 0; }
    bool operator==(const PortRef& o) const { return channel == o.channel && device == o.device; }
    bool operator!=(const PortRef& o) const { return !(*this == o); }
    bool operator<(const PortRef& o) const
    {
        const int c = device.compareNatural(o.device);
        return c != 0 ? c < 0 : channel < o.channel;
    }
};

// "name#ch" with the name URL-escaped, so any device name is safe (LiveRack's format).
juce::String encodePort(const PortRef&);
std::optional<PortRef> decodePort(const juce::String&);

} // namespace fs

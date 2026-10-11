#pragma once
#include <juce_core/juce_core.h>

namespace fs {

// How an interface is named in settings (PortRef::device). On macOS the plain CoreAudio name
// (unchanged since v1). On Windows / Linux "<driver>: <device>", e.g. "ASIO: Focusrite USB ASIO",
// so the same hardware seen through two drivers (ASIO and WASAPI) stays two interfaces.
struct InterfaceKey
{
    juce::String type, name;
};

juce::String makeInterfaceKey(const juce::String& type, const juce::String& name);

// Splits after a known driver prefix only, so device names containing ": " survive. A key with
// no known prefix is a CoreAudio name when CoreAudio is available, else the first driver's.
InterfaceKey splitInterfaceKey(const juce::String& key, const juce::StringArray& knownTypes);

} // namespace fs

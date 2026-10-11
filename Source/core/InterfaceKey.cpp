#include "core/InterfaceKey.h"

namespace fs {

namespace {
const juce::String kCoreAudio = "CoreAudio";
}

juce::String makeInterfaceKey(const juce::String& type, const juce::String& name)
{
    return type == kCoreAudio ? name : type + ": " + name;
}

InterfaceKey splitInterfaceKey(const juce::String& key, const juce::StringArray& knownTypes)
{
    for (const auto& type : knownTypes)
        if (type != kCoreAudio && key.startsWith(type + ": ")) return { type, key.substring(type.length() + 2) };
    if (knownTypes.contains(kCoreAudio) || knownTypes.isEmpty()) return { kCoreAudio, key };
    return { knownTypes[0], key };
}

} // namespace fs

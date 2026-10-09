#include "core/PortRef.h"

namespace fs {

juce::String encodePort(const PortRef& p)
{
    return juce::URL::addEscapeChars(p.device, false) + "#" + juce::String(p.channel);
}

std::optional<PortRef> decodePort(const juce::String& text)
{
    const int hash = text.lastIndexOfChar('#');
    if (hash <= 0) return std::nullopt;
    const auto number = text.substring(hash + 1).trim();
    if (number.isEmpty() || !number.containsOnly("0123456789")) return std::nullopt;
    return PortRef { juce::URL::removeEscapeChars(text.substring(0, hash)), number.getIntValue() };
}

} // namespace fs

#include "core/Settings.h"

namespace fs {

Settings& Settings::operator=(const Settings& o)
{
    if (this == &o) return *this;
    inputs = o.inputs;
    averaging = o.averaging;
    bandsPerOctave = o.bandsPerOctave;
    showBars = o.showBars;
    showDelta = o.showDelta;
    peaks = o.peaks;
    splOffset = o.splOffset;
    calibration = o.calibration;
    limitLAeq15 = o.limitLAeq15;
    limitLCeq15 = o.limitLCeq15;
    spectrogramSeconds = o.spectrogramSeconds;
    spectrogramFloor = o.spectrogramFloor;
    spectrogramTop = o.spectrogramTop;
    displayTop = o.displayTop;
    displayRange = o.displayRange;
    deviceState = o.deviceState ? std::make_unique<juce::XmlElement>(*o.deviceState) : nullptr;
    return *this;
}

std::unique_ptr<juce::XmlElement> Settings::toXml() const
{
    auto x = std::make_unique<juce::XmlElement>("FOHScope");
    x->setAttribute("version", 1);
    x->setAttribute("averaging", int(averaging));
    x->setAttribute("bandsPerOctave", bandsPerOctave);
    x->setAttribute("showBars", showBars);
    x->setAttribute("showDelta", showDelta);
    x->setAttribute("peakThreshold", double(peaks.thresholdDb));
    x->setAttribute("peakHold", double(peaks.holdSec));
    x->setAttribute("splOffset", splOffset);
    x->setAttribute("calibration", calibration);
    x->setAttribute("limitLAeq15", limitLAeq15);
    x->setAttribute("limitLCeq15", limitLCeq15);
    x->setAttribute("spectrogramSeconds", spectrogramSeconds);
    x->setAttribute("spectrogramFloor", double(spectrogramFloor));
    x->setAttribute("spectrogramTop", double(spectrogramTop));
    x->setAttribute("displayTop", double(displayTop));
    x->setAttribute("displayRange", double(displayRange));

    auto* in = x->createNewChildElement("Inputs");
    in->setAttribute("focused", inputs.focusedId);
    in->setAttribute("deltaA", inputs.deltaA);
    in->setAttribute("deltaB", inputs.deltaB);
    in->setAttribute("nextColour", inputs.nextColour);
    for (const auto& i : inputs.inputs) {
        auto* e = in->createNewChildElement("Input");
        e->setAttribute("id", i.id);
        e->setAttribute("name", i.name);
        e->setAttribute("channel", i.channel);
        e->setAttribute("colour", i.colour.toString());
        e->setAttribute("muted", i.muted);
        e->setAttribute("splMic", i.splMic);
    }
    if (deviceState) x->addChildElement(new juce::XmlElement(*deviceState));
    return x;
}

Settings Settings::fromXml(const juce::XmlElement& x)
{
    Settings s;
    s.averaging = Averaging(juce::jlimit(0, 2, x.getIntAttribute("averaging", int(s.averaging))));
    s.bandsPerOctave = x.getIntAttribute("bandsPerOctave", s.bandsPerOctave);
    s.showBars = x.getBoolAttribute("showBars", s.showBars);
    s.showDelta = x.getBoolAttribute("showDelta", s.showDelta);
    s.peaks.thresholdDb = float(x.getDoubleAttribute("peakThreshold", s.peaks.thresholdDb));
    s.peaks.holdSec = float(x.getDoubleAttribute("peakHold", s.peaks.holdSec));
    s.splOffset = x.getDoubleAttribute("splOffset", s.splOffset);
    s.calibration = x.getStringAttribute("calibration", s.calibration);
    s.limitLAeq15 = x.getDoubleAttribute("limitLAeq15", s.limitLAeq15);
    s.limitLCeq15 = x.getDoubleAttribute("limitLCeq15", s.limitLCeq15);
    s.spectrogramSeconds = x.getIntAttribute("spectrogramSeconds", s.spectrogramSeconds);
    s.spectrogramFloor = float(x.getDoubleAttribute("spectrogramFloor", s.spectrogramFloor));
    s.spectrogramTop = float(x.getDoubleAttribute("spectrogramTop", s.spectrogramTop));
    s.displayTop = float(x.getDoubleAttribute("displayTop", s.displayTop));
    s.displayRange = float(x.getDoubleAttribute("displayRange", s.displayRange));

    if (auto* in = x.getChildByName("Inputs")) {
        s.inputs.focusedId = in->getIntAttribute("focused");
        s.inputs.deltaA = in->getIntAttribute("deltaA", -1);
        s.inputs.deltaB = in->getIntAttribute("deltaB", -1);
        s.inputs.nextColour = in->getIntAttribute("nextColour");
        for (auto* e : in->getChildWithTagNameIterator("Input")) {
            if (int(s.inputs.inputs.size()) >= InputList::kMax) break;
            s.inputs.inputs.push_back({ e->getIntAttribute("id"), e->getStringAttribute("name"), juce::jmax(0, e->getIntAttribute("channel")),
                                        juce::Colour::fromString(e->getStringAttribute("colour")), e->getBoolAttribute("muted"),
                                        e->getBoolAttribute("splMic") });
        }
    }
    if (s.inputs.inputs.empty()) s.inputs.add(1);
    if (!s.inputs.find(s.inputs.focusedId)) s.inputs.focusedId = s.inputs.inputs.front().id;
    if (auto* d = x.getChildByName("DEVICESETUP")) s.deviceState = std::make_unique<juce::XmlElement>(*d);
    return s;
}

juce::File Settings::directory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Application Support/FOHScope");
}

Settings Settings::load()
{
    if (auto x = juce::XmlDocument::parse(directory().getChildFile("settings.xml")))
        if (x->hasTagName("FOHScope")) return fromXml(*x);
    Settings s;
    s.inputs.add(1);
    return s;
}

bool Settings::save() const
{
    const auto dir = directory();
    if (!dir.isDirectory() && !dir.createDirectory()) return false;
    return toXml()->writeTo(dir.getChildFile("settings.xml"));
}

} // namespace fs

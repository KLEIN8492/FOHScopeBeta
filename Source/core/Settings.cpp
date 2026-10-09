#include "core/Settings.h"

namespace fs {

std::unique_ptr<juce::XmlElement> Settings::toXml() const
{
    auto x = std::make_unique<juce::XmlElement>("FOHScope");
    x->setAttribute("version", 2);
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
    x->setAttribute("spectrogramFloorSpl", double(spectrogramFloorSpl));
    x->setAttribute("spectrogramTopSpl", double(spectrogramTopSpl));
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
        e->setAttribute("port", i.port.valid() ? encodePort(i.port) : juce::String());
        e->setAttribute("colour", i.colour.toString());
        e->setAttribute("muted", i.muted);
        e->setAttribute("splMic", i.splMic);
    }
    auto* audio = x->createNewChildElement("Audio");
    audio->setAttribute("rate", sampleRate);
    audio->setAttribute("buffer", bufferSize);
    audio->setAttribute("lastDevice", lastDevice);
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
    s.spectrogramFloorSpl = float(x.getDoubleAttribute("spectrogramFloorSpl", s.spectrogramFloorSpl));
    s.spectrogramTopSpl = float(x.getDoubleAttribute("spectrogramTopSpl", s.spectrogramTopSpl));
    s.displayTop = float(x.getDoubleAttribute("displayTop", s.displayTop));
    s.displayRange = float(x.getDoubleAttribute("displayRange", s.displayRange));

    // v1 stored a bare channel of the one device in DEVICESETUP; v2 stores a port per input.
    juce::String v1Device;
    if (auto* d = x.getChildByName("DEVICESETUP")) {
        v1Device = d->getStringAttribute("audioInputDeviceName");
        s.sampleRate = d->getDoubleAttribute("audioDeviceRate", s.sampleRate);
        s.bufferSize = d->getIntAttribute("audioDeviceBufferSize", s.bufferSize);
        s.lastDevice = v1Device;
    }
    if (auto* a = x.getChildByName("Audio")) {
        s.sampleRate = a->getDoubleAttribute("rate", s.sampleRate);
        s.bufferSize = a->getIntAttribute("buffer", s.bufferSize);
        s.lastDevice = a->getStringAttribute("lastDevice", s.lastDevice);
    }

    if (auto* in = x.getChildByName("Inputs")) {
        s.inputs.focusedId = in->getIntAttribute("focused");
        s.inputs.deltaA = in->getIntAttribute("deltaA", -1);
        s.inputs.deltaB = in->getIntAttribute("deltaB", -1);
        s.inputs.nextColour = in->getIntAttribute("nextColour");
        for (auto* e : in->getChildWithTagNameIterator("Input")) {
            if (int(s.inputs.inputs.size()) >= InputList::kMax) break;
            PortRef port;
            if (e->hasAttribute("port")) {
                if (auto p = decodePort(e->getStringAttribute("port"))) port = *p;
            } else {
                port = { v1Device, juce::jmax(0, e->getIntAttribute("channel")) };
            }
            s.inputs.inputs.push_back({ e->getIntAttribute("id"), e->getStringAttribute("name"), port,
                                        juce::Colour::fromString(e->getStringAttribute("colour")), e->getBoolAttribute("muted"),
                                        e->getBoolAttribute("splMic") });
        }
    }
    if (!s.inputs.find(s.inputs.focusedId)) s.inputs.focusedId = s.inputs.inputs.empty() ? 0 : s.inputs.inputs.front().id;
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
    return {};
}

bool Settings::save() const
{
    const auto dir = directory();
    if (!dir.isDirectory() && !dir.createDirectory()) return false;
    return toXml()->writeTo(dir.getChildFile("settings.xml"));
}

} // namespace fs

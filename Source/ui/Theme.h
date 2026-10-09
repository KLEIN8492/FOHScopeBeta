#pragma once
#include "dsp/FreqGrid.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>

// Same house style as VenueTuner / LiveRack: raw, dense, dark. Flat grey, monospace, square
// corners, 1px rules, uppercase captions. Colour is reserved for traces and state.
namespace fs::theme {

inline const juce::Colour bg { 0xff161718 }, panel { 0xff1d1f21 }, well { 0xff111213 }, statusBg { 0xff0f1011 },
    line { 0xff2c2f33 }, gridLine { 0xff24272a }, gridStrong { 0xff34383d }, text { 0xffd6d8da }, dim { 0xff7c8186 },
    accent { 0xff4fd1c5 }, ok { 0xff46a758 }, amber { 0xffe0b341 }, red { 0xffe5484d }, capture { 0xff9aa4b2 };

// Source literals are UTF-8; juce::String(const char*) assumes ASCII, so wrap any non-ASCII text.
inline juce::String utf8(const char* s) { return juce::String::fromUTF8(s); }

// dB re FS RMS (internal) → dBFS as shown (AES17: a full-scale sine reads 0 dBFS).
constexpr float kAes17 = 3.0103f;

inline juce::Font mono(float h, bool bold = false) { return juce::Font(juce::FontOptions("Menlo", h, bold ? juce::Font::bold : juce::Font::plain)); }

// Small uppercase caption: section titles, column headers, field labels.
inline void caption(juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, juce::Justification j = juce::Justification::centredLeft)
{
    g.setColour(dim);
    g.setFont(mono(10.0f, true));
    g.drawText(s.toUpperCase(), r, j, true);
}

// Square colour swatch with a dark rim.
inline void swatch(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c)
{
    g.setColour(c);
    g.fillRect(r);
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawRect(r);
}

// Segmented horizontal peak meter: green to −12 dB, amber to −3 dB, red above (as LiveRack).
inline void meter(juce::Graphics& g, juce::Rectangle<int> r, float db)
{
    g.setColour(well);
    g.fillRect(r);
    const int segments = std::max(4, r.getWidth() / 5);
    const int lit = juce::roundToInt(juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f) * float(segments));
    const float segW = float(r.getWidth()) / float(segments);
    for (int i = 0; i < segments; ++i) {
        const float segDb = -60.0f + 60.0f * (float(i) + 1.0f) / float(segments);
        const auto c = segDb > -3.0f ? red : segDb > -12.0f ? amber : ok;
        g.setColour(i < lit ? c : c.withAlpha(0.12f));
        g.fillRect(juce::Rectangle<float>(float(r.getX()) + float(i) * segW, float(r.getY()), segW - 1.0f, float(r.getHeight())));
    }
}

// Plot insets shared by the spectrum and the spectrograph so their frequency axes line up.
constexpr int kPlotLeft = 46, kPlotRight = 14;

// Log frequency axis 20 Hz – 20 kHz across a pixel span.
struct FreqAxis
{
    float x0 = 0, x1 = 1;
    static constexpr double lo = 20.0, hi = 20000.0;
    float x(double hz) const { return x0 + (x1 - x0) * float(std::log(hz / lo) / std::log(hi / lo)); }
    double hz(float px) const { return lo * std::pow(hi / lo, double((px - x0) / (x1 - x0))); }
};

inline juce::String hzText(double hz)
{
    return hz >= 1000.0 ? juce::String(hz / 1000.0, hz >= 10000.0 ? 1 : 2) + "k" : juce::String(juce::roundToInt(hz));
}

// Spectrograph colour map (black → violet → red → orange → yellow → white), 256 entries.
inline const std::array<juce::PixelARGB, 256>& heatMap()
{
    static const auto lut = [] {
        std::array<juce::PixelARGB, 256> t {};
        const juce::Colour stops[] = { juce::Colour(0xff000000), juce::Colour(0xff1b0c41), juce::Colour(0xff6a176e), juce::Colour(0xffbb3754),
                                       juce::Colour(0xfff1711f), juce::Colour(0xfffbc72a), juce::Colour(0xfffcffa4) };
        constexpr int n = int(std::size(stops)) - 1;
        for (int i = 0; i < 256; ++i) {
            const float p = float(i) / 255.0f * n;
            const int s = std::min(int(p), n - 1);
            t[size_t(i)] = stops[s].interpolatedWith(stops[s + 1], p - float(s)).getPixelARGB();
        }
        return t;
    }();
    return lut;
}

// Flat, square, monospace widgets everywhere (buttons, combos, sliders, menus, tabs, dialogs).
class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColourScheme({ bg, panel, well, line, text, accent, juce::Colours::black, well, text });
        setColour(juce::ResizableWindow::backgroundColourId, bg);
        setColour(juce::DocumentWindow::backgroundColourId, bg);
        setColour(juce::TextButton::buttonColourId, well);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        setColour(juce::ComboBox::backgroundColourId, well);
        setColour(juce::ComboBox::outlineColourId, line);
        setColour(juce::ComboBox::arrowColourId, dim);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::TextEditor::backgroundColourId, well);
        setColour(juce::TextEditor::outlineColourId, line);
        setColour(juce::TextEditor::focusedOutlineColourId, accent);
        setColour(juce::TextEditor::textColourId, text);
        setColour(juce::Label::textColourId, text);
        setColour(juce::ListBox::backgroundColourId, panel);
        setColour(juce::PopupMenu::backgroundColourId, panel);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, line);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedTextColourId, text);
        setColour(juce::ScrollBar::thumbColourId, line);
        setColour(juce::TooltipWindow::backgroundColourId, panel);
        setColour(juce::TooltipWindow::outlineColourId, line);
        setColour(juce::TooltipWindow::textColourId, text);
        setColour(juce::Slider::textBoxBackgroundColourId, well);
        setColour(juce::Slider::textBoxOutlineColourId, line);
        setColour(juce::Slider::textBoxTextColourId, text);
        setColour(juce::AlertWindow::backgroundColourId, panel);
        setColour(juce::AlertWindow::outlineColourId, line);
        setColour(juce::AlertWindow::textColourId, text);
        setColour(juce::TabbedComponent::outlineColourId, line);
        setColour(juce::TabbedComponent::backgroundColourId, panel);
        setColour(juce::TabbedButtonBar::tabOutlineColourId, line);
        setColour(juce::TabbedButtonBar::frontOutlineColourId, line);
        setColour(juce::ListBox::outlineColourId, line);
    }

    juce::Typeface::Ptr getTypefaceForFont(const juce::Font& f) override
    {
        if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName()) {
            auto m = f;
            m.setTypefaceName("Menlo");
            return juce::Typeface::createSystemTypefaceFor(m);
        }
        return LookAndFeel_V4::getTypefaceForFont(f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& colour, bool over, bool down) override
    {
        auto r = b.getLocalBounds();
        const bool on = b.getToggleState();
        g.setColour(down ? colour.darker(0.25f) : colour);
        g.fillRect(r);
        g.setColour(on ? colour : (over ? dim : line));
        g.drawRect(r);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        g.setColour(b.findColour(on ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                        .withMultipliedAlpha(b.isEnabled() ? 1.0f : 0.4f));
        g.setFont(getTextButtonFont(b, b.getHeight()));
        g.drawText(b.getButtonText().toUpperCase(), b.getLocalBounds().reduced(4, 0), juce::Justification::centred, false);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int) override { return mono(11.0f); }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return mono(11.0f); }
    juce::Font getPopupMenuFont() override { return mono(11.5f); }
    juce::Font getLabelFont(juce::Label&) override { return mono(11.0f); }
    juce::Font getAlertWindowMessageFont() override { return mono(11.5f); }
    juce::Font getAlertWindowTitleFont() override { return mono(12.0f, true); }
    juce::Font getTabButtonFont(juce::TabBarButton&, float) override { return mono(11.0f); }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        g.setColour(well);
        g.fillRect(0, 0, w, h);
        g.setColour(line);
        g.drawRect(0, 0, w, h);
        const float ax = float(w) - 12.0f, ay = float(h) * 0.5f;
        juce::Path p;
        p.addTriangle(ax - 4.0f, ay - 2.0f, ax + 4.0f, ay - 2.0f, ax, ay + 3.0f);
        g.setColour(dim);
        g.fillPath(p);
    }

    void drawPopupMenuBackground(juce::Graphics& g, int w, int h) override
    {
        g.fillAll(panel);
        g.setColour(line);
        g.drawRect(0, 0, w, h);
    }

    void fillTextEditorBackground(juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        g.setColour(e.findColour(juce::TextEditor::backgroundColourId));
        g.fillRect(0, 0, w, h);
    }

    void drawTextEditorOutline(juce::Graphics& g, int w, int h, juce::TextEditor& e) override
    {
        g.setColour(e.hasKeyboardFocus(true) ? accent : line);
        g.drawRect(0, 0, w, h);
    }

    void drawTooltip(juce::Graphics& g, const juce::String& t, int w, int h) override
    {
        g.fillAll(panel);
        g.setColour(line);
        g.drawRect(0, 0, w, h);
        g.setColour(text);
        g.setFont(mono(11.0f));
        g.drawFittedText(t, juce::Rectangle<int>(w, h).reduced(6, 3), juce::Justification::centredLeft, 4);
    }

    // Thin track, accent fill to the value, square thumb.
    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle,
                          juce::Slider& s) override
    {
        const int cy = y + h / 2;
        g.setColour(well);
        g.fillRect(x, cy - 2, w, 4);
        g.setColour(line);
        g.drawRect(x, cy - 2, w, 4);
        g.setColour(s.isEnabled() ? accent.withAlpha(0.7f) : dim);
        g.fillRect(float(x), float(cy - 1), pos - float(x), 2.0f);
        g.setColour(text);
        g.fillRect(juce::Rectangle<float>(pos - 3.0f, float(cy - 7), 6.0f, 14.0f));
    }

    void drawTabButton(juce::TabBarButton& b, juce::Graphics& g, bool over, bool) override
    {
        auto r = b.getLocalBounds();
        const bool front = b.isFrontTab();
        g.setColour(front ? panel : statusBg);
        g.fillRect(r);
        g.setColour(line);
        g.drawRect(r);
        if (front) {
            g.setColour(accent);
            g.fillRect(r.removeFromTop(2));
        }
        g.setColour(front ? accent : (over ? text : dim));
        g.setFont(mono(11.0f));
        g.drawText(b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred);
    }

    int getTabButtonBestWidth(juce::TabBarButton& b, int) override { return 40 + b.getButtonText().length() * 8; }
    void drawTabbedButtonBarBackground(juce::TabbedButtonBar&, juce::Graphics& g) override { g.fillAll(statusBg); }
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&, juce::Graphics&, int, int) override {}

    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& w, const juce::Rectangle<int>& textArea, juce::TextLayout& layout) override
    {
        g.fillAll(panel);
        g.setColour(line);
        g.drawRect(w.getLocalBounds());
        g.setColour(text);
        layout.draw(g, textArea.toFloat());
    }

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over,
                       bool) override
    {
        g.setColour(over ? dim : line);
        if (vertical) g.fillRect(x + 2, thumbStart, w - 4, thumbSize);
        else g.fillRect(thumbStart, y + 2, thumbSize, h - 4);
    }
};

} // namespace fs::theme

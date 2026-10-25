#pragma once
#include <juce_graphics/juce_graphics.h>
#include <array>

// The one place note colours live. Every view (pads, keys, lanes, labels) takes them from here.
namespace pc::gui::colours
{
    // C to F follow Basspotato's palette (yellow, orange, pink, purple, blue, sky);
    // F# to B are bright greens in the same style, so the octave loops back to yellow.
    inline const std::array<juce::Colour, 12> notes {
        juce::Colour (0xffFED031),  // C  yellow
        juce::Colour (0xffFDAF5B),  // C# orange
        juce::Colour (0xffFB5EA8),  // D  pink
        juce::Colour (0xffBD5EDA),  // D# purple
        juce::Colour (0xff68A5DE),  // E  blue
        juce::Colour (0xff46C0DF),  // F  sky
        juce::Colour (0xff2FD8C8),  // F# aqua
        juce::Colour (0xff3FE08F),  // G  mint
        juce::Colour (0xff6EE65A),  // G# green
        juce::Colour (0xffB2EA3C),  // A  lime
        juce::Colour (0xffDCE036),  // A# gold-lime
        juce::Colour (0xffFFC23A),  // B  amber
    };

    inline juce::Colour forNote (int noteOrPitchClass) { return notes[(size_t) (((noteOrPitchClass % 12) + 12) % 12)]; }

    // Interface colours (not note colours)
    inline const juce::Colour background  { 0xff15122b };  // deep indigo
    inline const juce::Colour backgroundTop    { 0xff221c4a };
    inline const juce::Colour backgroundBottom { 0xff0d0a1e };
    inline const juce::Colour chassis     { 0xff1c1738 };  // the raised body of the "device"
    inline const juce::Colour well        { 0xff120f26 };  // recessed areas
    inline const juce::Colour screen      { 0xff0b0a1c };  // the display glass
    inline const juce::Colour whiteKey    { 0xfff2effa };
    inline const juce::Colour blackKey    { 0xff2a2545 };
    inline const juce::Colour panel       { 0xff221d45 };
    inline const juce::Colour panelLight  { 0xff302a5e };
    inline const juce::Colour outline     { 0xff433b7d };
    inline const juce::Colour text        { 0xfff4f1ff };
    inline const juce::Colour textDim     { 0xff9d96c8 };
    inline const juce::Colour accent      { 0xffFA3DC7 };  // hot pink from the palette, for selected controls
    inline const juce::Colour onNote      { 0xff15122b };  // text drawn on top of a note colour
}

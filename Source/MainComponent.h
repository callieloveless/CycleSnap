/*
  ==============================================================================
    MainComponent.h

    CycleSnap UI.
    Provides controls for geometric variables (N, s, R) and file I/O.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "MidiTransformEngine.h"

class MainComponent : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Drag & Drop
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    MidiTransformEngine engine;
    bool isDragActive = false;

    // UI Logic
    void loadFile(const juce::File& f);
    void runSolver();
    void generate();
    void save();
    void log(const juce::String& m);
    void updateInputStates();

    // Components
    juce::Label lblN{ "lblN", "REPETITIONS (N)" },
        lblS{ "lblS", "SCALE (s)" },
        lblR{ "lblR", "TOTAL RATIO (R)" },
        lblEnd{ "lblEnd", "END SCALE" };

    juce::TextEditor inputN, inputS, inputR, inputEnd;
    juce::Label lblMode{ "lblMode", "CALCULATION MODE" };

    juce::ToggleButton radioTgt{ "Target Duration" },
        radioAccel{ "Fixed Accel" },
        radioFinal{ "Match End Scale" },
        radioCurve{ "Fit Curve" };

    juce::TextButton btnLoad{ "LOAD MIDI" },
        btnSolve{ "CALCULATE" },
        btnGen{ "GENERATE" },
        btnSave{ "SAVE MIDI" };

    juce::ToggleButton tglDump{ "Dump Debug .txt" };
    juce::TextEditor logEditor;

    // Styling
    juce::Font terminalFont{ "Courier New", 14.0f, juce::Font::bold };
    // Theme colors
    const juce::Colour cBackground{ 0xff050505 };
    const juce::Colour cAccent{ 0xff00ff41 };    // Terminal Green
    const juce::Colour cSecondary{ 0xff00ffff }; // Cyan
    const juce::Colour cDim{ 0xff004411 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
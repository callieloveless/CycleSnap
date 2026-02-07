/*
  ==============================================================================
    MainComponent.h

    CycleSnap Main UI.
  ==============================================================================
*/
#pragma once

#include <JuceHeader.h>
#include "MidiTransformEngine.h"

class MainComponent : public juce::Component,
    public juce::FileDragAndDropTarget
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Drag & Drop
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    // Logic
    void loadFile(const juce::File& file);
    void runSolver();
    void generate();
    void saveFile();
    void logMessage(const juce::String& msg);
    void updateInputStates();
    void updateErrorDisplay(double errorMs);

    MidiTransformEngine engine;
    bool isDragActive = false;

    // UI Components
    juce::TooltipWindow tooltipWindow{ this, 700 };

    // Parameters
    juce::Label lblN{ "lblN", "REPETITIONS [N]" };
    juce::TextEditor inputN;

    juce::Label lblS{ "lblS", "BEAT RATIO [s]" };
    juce::TextEditor inputS;

    juce::Label lblR{ "lblR", "TOTAL SCALE [R]" };
    juce::TextEditor inputR;

    juce::Label lblSend{ "lblSend", "BEAT END [E]" };
    juce::TextEditor inputSend;

    juce::Label lblMode{ "lblMode", "OPERATION MODE" };
    juce::ComboBox modeSelector;
    juce::ToggleButton chkIntLoops{ "INT LOOPS LOCK" };

    // Monitoring
    juce::Label lblErrorMonitor;
    juce::TextEditor logEditor;

    // Actions
    juce::TextButton loadButton{ "LOAD SOURCE" };
    juce::TextButton ejectButton{ "EJECT" };

    juce::TextButton solveButton{ "CALCULATE" };
    juce::TextButton generateButton{ "GENERATE" };
    juce::TextButton saveButton{ "SAVE DISK" };

    juce::ToggleButton debugDumpToggle{ "DUMP .TXT" };

    // Styles & Theme
    juce::Font headerFont{ "Courier New", 18.0f, juce::Font::bold };
    juce::Font dataFont{ "Courier New", 14.0f, juce::Font::bold };
    juce::Font labelFont{ "Courier New", 12.0f, juce::Font::plain };

    const juce::Colour cBackground{ 0xff050505 };
    const juce::Colour cPanel{ 0xff0a0a0a };
    const juce::Colour cFrame{ 0xff004411 };
    const juce::Colour cCyan{ 0xff00ffff };
    const juce::Colour cGreen{ 0xff00ff41 };
    const juce::Colour cOrange{ 0xffffaa00 };
    const juce::Colour cRed{ 0xffff0000 };

    void drawChamferedPanel(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title, juce::Colour color);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
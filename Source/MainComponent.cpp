/*
  ==============================================================================
    MainComponent.cpp
  ==============================================================================
*/

#include "MainComponent.h"

MainComponent::MainComponent()
{
    // --- UI Helper Lambdas ---
    auto setupInput = [&](juce::TextEditor& e, const juce::String& defaultVal) {
        addAndMakeVisible(e);
        e.setJustification(juce::Justification::centred);
        e.setInputRestrictions(10, "0123456789.");
        e.applyFontToAllText(terminalFont);
        e.setText(defaultVal);

        // High contrast style
        e.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
        e.setColour(juce::TextEditor::textColourId, cSecondary);
        e.setColour(juce::TextEditor::outlineColourId, cDim);
        e.setColour(juce::TextEditor::focusedOutlineColourId, cAccent);
        };

    auto setupLabel = [&](juce::Label& l) {
        addAndMakeVisible(l);
        l.setFont(terminalFont);
        l.setColour(juce::Label::textColourId, cAccent);
        l.setJustificationType(juce::Justification::centred);
        };

    // Parameter Controls
    setupLabel(lblN); setupInput(inputN, "4");
    setupLabel(lblS); setupInput(inputS, "1.05");
    setupLabel(lblR); setupInput(inputR, "2.0");
    setupLabel(lblEnd); setupInput(inputEnd, "2.0");

    // Mode Selection
    addAndMakeVisible(lblMode);
    lblMode.setColour(juce::Label::backgroundColourId, cDim);

    auto setupRadio = [&](juce::ToggleButton& b) {
        addAndMakeVisible(b);
        b.setRadioGroupId(1001);
        b.setColour(juce::ToggleButton::textColourId, cAccent);
        b.onClick = [this] { updateInputStates(); };
        };
    setupRadio(radioTgt); setupRadio(radioAccel); setupRadio(radioFinal); setupRadio(radioCurve);
    radioTgt.setToggleState(true, juce::dontSendNotification);

    // Buttons
    auto setupBtn = [&](juce::TextButton& b, juce::Colour textColor) {
        addAndMakeVisible(b);
        b.setColour(juce::TextButton::buttonColourId, juce::Colours::black);
        b.setColour(juce::TextButton::textColourOffId, textColor);
        b.setColour(juce::TextButton::buttonOnColourId, textColor);
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
        };

    addChildComponent(btnLoad); // Hidden, handled via Drag/Drop mostly
    btnLoad.onClick = [this] {
        auto fc = std::make_shared<juce::FileChooser>("Open MIDI", juce::File(), "*.mid");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fc](const juce::FileChooser& c) {
                if (c.getResult().exists()) loadFile(c.getResult());
            });
        };

    setupBtn(btnSolve, cSecondary);
    btnSolve.onClick = [this] { runSolver(); };

    setupBtn(btnGen, cAccent);
    btnGen.onClick = [this] { generate(); };
    btnGen.setEnabled(false);

    setupBtn(btnSave, juce::Colours::orange);
    btnSave.onClick = [this] { save(); };
    btnSave.setEnabled(false);

    addAndMakeVisible(tglDump);
    tglDump.setColour(juce::ToggleButton::textColourId, juce::Colours::grey);

    // Console Log
    addAndMakeVisible(logEditor);
    logEditor.setMultiLine(true);
    logEditor.setReadOnly(true);
    logEditor.setCaretVisible(false);
    logEditor.setFont(terminalFont.withHeight(12.0f));
    logEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    logEditor.setColour(juce::TextEditor::textColourId, cAccent);
    logEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);

    logEditor.setText(">> SYSTEM READY.\n>> WAITING FOR SOURCE FILE...\n");

    setSize(600, 720);
    updateInputStates();
}

void MainComponent::updateInputStates() {
    auto setEnabled = [&](juce::Component& c, bool en) {
        c.setEnabled(en);
        c.setAlpha(en ? 1.0f : 0.3f);
        };

    bool isTgt = radioTgt.getToggleState();
    bool isAccel = radioAccel.getToggleState();
    bool isFinal = radioFinal.getToggleState();
    bool isCurve = radioCurve.getToggleState();

    setEnabled(inputN, isTgt || isAccel || isFinal);
    setEnabled(inputS, isAccel || isCurve);
    setEnabled(inputR, isTgt || isCurve);
    setEnabled(inputEnd, isFinal);
}

void MainComponent::log(const juce::String& m) {
    logEditor.moveCaretToEnd();
    logEditor.insertTextAtCaret(">> " + m + "\n");
}

void MainComponent::loadFile(const juce::File& f) {
    log("LOADING: " + f.getFileName());
    auto res = engine.loadSource(f);

    if (res.wasOk()) {
        log("LOADED OK.");
        btnGen.setEnabled(false);
        btnSave.setEnabled(false);
    }
    else {
        log("ERROR: " + res.getErrorMessage());
    }
    repaint();
}

void MainComponent::runSolver() {
    if (!engine.isSourceLoaded()) {
        log("ERROR: No source file loaded.");
        return;
    }
    log("CALCULATING PARAMETERS...");

    double reps = inputN.getText().getDoubleValue();
    double s = inputS.getText().getDoubleValue();
    double R = inputR.getText().getDoubleValue();
    double end = inputEnd.getText().getDoubleValue();

    auto mode = GeoTimeMath::Mode::TargetDuration;
    if (radioAccel.getToggleState()) mode = GeoTimeMath::Mode::FixedAccel;
    if (radioFinal.getToggleState()) mode = GeoTimeMath::Mode::MatchEndScale;
    if (radioCurve.getToggleState()) mode = GeoTimeMath::Mode::FitToCurve;

    auto res = engine.runSolver(mode, reps, s, R, end);

    if (res.success) {
        log(juce::String("SOLVED: N=") + juce::String(res.repetitions) +
            " s=" + juce::String(res.scaleFactor, 5));

        // Feedback values to UI
        inputN.setText(juce::String(res.repetitions));
        inputS.setText(juce::String(res.scaleFactor, 5));
        inputR.setText(juce::String(res.timeRatio, 5));
        inputEnd.setText(juce::String(res.terminalScale, 5));

        btnGen.setEnabled(true);
    }
    else {
        log("SOLVER FAILED: " + juce::String(res.message));
    }
}

void MainComponent::generate() {
    double n = inputN.getText().getDoubleValue();
    int m = engine.getSegmentCount();
    if (m < 1) m = 1;

    int totalSteps = (int)std::round(n * m);

    auto res = engine.generateOutput(totalSteps, inputS.getText().getDoubleValue());
    if (res.wasOk()) {
        log("SEQUENCE GENERATED.");
        btnSave.setEnabled(true);
    }
    else {
        log("GEN FAILED: " + res.getErrorMessage());
    }
}

void MainComponent::save() {
    auto fc = std::make_shared<juce::FileChooser>("Save Output",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.mid");

    fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser& c) {
            auto f = c.getResult();
            if (f != juce::File()) {
                if (engine.saveFile(f).wasOk()) {
                    log("SAVED: " + f.getFileName());

                    if (tglDump.getToggleState()) {
                        f.withFileExtension("txt").replaceWithText(engine.getDebugDump());
                    }
                }
                else {
                    log("SAVE FAILED.");
                }
            }
        });
}

// Drag & Drop
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& f) {
    return f.size() == 1 && f[0].endsWithIgnoreCase(".mid");
}
void MainComponent::fileDragEnter(const juce::StringArray&, int, int) { isDragActive = true; repaint(); }
void MainComponent::fileDragExit(const juce::StringArray&) { isDragActive = false; repaint(); }
void MainComponent::filesDropped(const juce::StringArray& f, int, int) {
    isDragActive = false;
    repaint();
    if (f.size() > 0) loadFile(juce::File(f[0]));
}

// Paint
void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(cBackground);
    auto area = getLocalBounds().toFloat();
    float m = 20.f;

    // Title
    auto head = area.removeFromTop(40);
    g.setFont(terminalFont.withHeight(24));
    g.setColour(juce::Colours::white);
    g.drawText("CycleSnap", head, juce::Justification::centred, true);

    // Drop Zone
    auto drop = area.removeFromTop(110).reduced(m, 0);

    // Draw grid/target styling
    if (isDragActive) {
        g.setColour(cSecondary);
        g.drawRect(drop, 3.f);
        g.setColour(cSecondary.withAlpha(0.1f));
        g.fillRect(drop);
        g.drawText(">> RELEASE TO LOAD <<", drop, juce::Justification::centred, true);
    }
    else {
        g.setColour(cDim);
        juce::Path p; p.addRectangle(drop);
        float dashes[] = { 8.f, 6.f };
        juce::PathStrokeType(2.f).createDashedStroke(p, p, dashes, 2);
        g.fillPath(p);

        if (engine.isSourceLoaded()) {
            g.setColour(cAccent);
            g.setFont(terminalFont.withHeight(16));
            g.drawText("SOURCE LOADED", drop.removeFromTop(30), juce::Justification::centred, true);

            g.setColour(cSecondary);
            g.setFont(terminalFont.withHeight(12));
            juce::String stats = "TRK: " + juce::String(engine.getSourceTrackCount()) +
                " | BPM: " + juce::String(engine.getSourceBPM());
            g.drawText(stats, drop, juce::Justification::centred, true);
        }
        else {
            g.setColour(juce::Colours::grey);
            g.setFont(terminalFont.withHeight(16));
            g.drawText("DROP MIDI HERE", drop, juce::Justification::centred, true);
        }
    }

    // Separator
    g.setColour(cDim);
    g.drawLine(m, area.getY() + 15, getWidth() - m, area.getY() + 15, 1.f);
}

void MainComponent::resized() {
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(30); // Title buffer

    // Drop Zone
    area.removeFromTop(110);

    // Controls Deck
    area.removeFromTop(30);
    auto deck = area.removeFromTop(220);

    // Columns
    auto left = deck.removeFromLeft(getWidth() / 2 - 10);
    lblMode.setBounds(left.removeFromTop(25));
    left.removeFromTop(5);

    radioTgt.setBounds(left.removeFromTop(30));
    radioAccel.setBounds(left.removeFromTop(30));
    radioFinal.setBounds(left.removeFromTop(30));
    radioCurve.setBounds(left.removeFromTop(30));

    deck.removeFromLeft(20);
    auto right = deck;
    int h = 60;

    auto row1 = right.removeFromTop(h);
    lblN.setBounds(row1.removeFromLeft(60).removeFromTop(20));
    inputN.setBounds(row1.removeFromLeft(60).removeFromTop(30));
    row1.removeFromLeft(10);
    lblS.setBounds(row1.removeFromLeft(60).removeFromTop(20));
    inputS.setBounds(row1.removeFromLeft(60).removeFromTop(30));

    right.removeFromTop(10);

    auto row2 = right.removeFromTop(h);
    lblR.setBounds(row2.removeFromLeft(60).removeFromTop(20));
    inputR.setBounds(row2.removeFromLeft(60).removeFromTop(30));
    row2.removeFromLeft(10);
    lblEnd.setBounds(row2.removeFromLeft(60).removeFromTop(20));
    inputEnd.setBounds(row2.removeFromLeft(60).removeFromTop(30));

    area.removeFromTop(20);

    // Bottom Actions
    auto btnRow = area.removeFromTop(40);
    btnSolve.setBounds(btnRow.removeFromLeft(120));
    btnRow.removeFromLeft(10);
    btnGen.setBounds(btnRow.removeFromLeft(120));
    btnRow.removeFromLeft(10);
    btnSave.setBounds(btnRow.removeFromLeft(120));
    tglDump.setBounds(btnRow.removeFromRight(100));

    area.removeFromTop(20);
    logEditor.setBounds(area);
}
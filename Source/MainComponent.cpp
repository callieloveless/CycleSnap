/*
  ==============================================================================
    MainComponent.cpp
  ==============================================================================
*/

#include "MainComponent.h"

MainComponent::MainComponent()
{
    // --- Styles & Setup ---
    auto setupEditor = [&](juce::TextEditor& e, const juce::String& tip) {
        addAndMakeVisible(e);
        e.setJustification(juce::Justification::centred);
        e.setInputRestrictions(10, "0123456789.");
        e.applyFontToAllText(dataFont);
        e.setTooltip(tip);

        // High contrast / Terminal look
        e.setColour(juce::TextEditor::backgroundColourId, cBackground);
        e.setColour(juce::TextEditor::textColourId, cCyan);
        e.setColour(juce::TextEditor::outlineColourId, cFrame);
        e.setColour(juce::TextEditor::focusedOutlineColourId, cGreen);
        };

    auto setupLabel = [&](juce::Label& l) {
        addAndMakeVisible(l);
        l.setFont(labelFont);
        l.setColour(juce::Label::textColourId, cGreen.withAlpha(0.8f));
        l.setJustificationType(juce::Justification::centredLeft);
        };

    // --- Controls Panel ---
    setupLabel(lblN);
    setupEditor(inputN, "Repetitions (N).\nTotal number of times the pattern plays.\nExample: 4.0 = 4 full loops.");
    inputN.setText("4");

    setupLabel(lblS);
    setupEditor(inputS, "Beat Ratio (s).\nGeometric multiplier per loop.\n> 1.0 = Slow Down (Decel)\n< 1.0 = Speed Up (Accel)");
    inputS.setText("1.5");

    setupLabel(lblR);
    setupEditor(inputR, "Total Scale (R).\nRatio of Output Duration vs Input Duration.\n2.0 = Output is twice as long.");
    inputR.setText("2.0");

    setupLabel(lblSend);
    setupEditor(inputSend, "Beat End (E).\nRelative scale of the LAST note compared to the first.\n2.0 = Last note is 2x longer.");
    inputSend.setText("2.0");

    addAndMakeVisible(modeSelector);
    modeSelector.addItem("LOOP TARGET [Fix N, R]", 1);
    modeSelector.addItem("LOOP ACCEL  [Fix N, s]", 2);
    modeSelector.addItem("LOOP FINAL  [Fix N, E]", 3);
    modeSelector.addItem("CURVE FIT   [Fix s, R]", 4);
    modeSelector.addItem("END FIT     [Fix E, R]", 5);

    modeSelector.setColour(juce::ComboBox::backgroundColourId, cBackground);
    modeSelector.setColour(juce::ComboBox::textColourId, cCyan);
    modeSelector.setColour(juce::ComboBox::outlineColourId, cFrame);
    modeSelector.setColour(juce::ComboBox::arrowColourId, cCyan);
    modeSelector.setTooltip("Select which variables are locked (Inputs) and which one to solve for.");
    modeSelector.onChange = [this] { updateInputStates(); };
    modeSelector.setSelectedId(1);

    addAndMakeVisible(chkIntLoops);
    chkIntLoops.setColour(juce::ToggleButton::textColourId, cGreen);
    chkIntLoops.setColour(juce::ToggleButton::tickColourId, cCyan);
    chkIntLoops.setToggleState(true, juce::dontSendNotification);
    chkIntLoops.setTooltip("If checked, 'Repetitions' will be rounded to the nearest whole number (full loops only).");

    // --- Log Panel ---
    addAndMakeVisible(logEditor);
    logEditor.setMultiLine(true);
    logEditor.setReadOnly(true);
    logEditor.setCaretVisible(false);
    logEditor.setFont(labelFont);
    logEditor.setColour(juce::TextEditor::backgroundColourId, cBackground);
    logEditor.setColour(juce::TextEditor::textColourId, cGreen);
    logEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    logEditor.setText(">> SYSTEM INITIALIZED.\n>> AWAITING INPUT...\n");

    addAndMakeVisible(lblErrorMonitor);
    lblErrorMonitor.setFont(dataFont);
    lblErrorMonitor.setJustificationType(juce::Justification::centredRight);
    lblErrorMonitor.setText("DRIFT: --", juce::dontSendNotification);
    lblErrorMonitor.setColour(juce::Label::textColourId, cFrame);
    lblErrorMonitor.setTooltip("Quantization Error (Drift).\n<10ms: Tight (Green)\n<30ms: Loose (Orange)\n>30ms: Error (Red)");

    // --- Data Panel ---
    addChildComponent(loadButton); // Clickable overlay for drag zone
    loadButton.setTooltip("Click or Drop MIDI file here.");
    loadButton.onClick = [this] {
        auto fc = std::make_shared<juce::FileChooser>("Open MIDI", juce::File(), "*.mid");
        fc->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, fc](const juce::FileChooser& c) { if (c.getResult().exists()) loadFile(c.getResult()); });
        };

    addAndMakeVisible(ejectButton);
    ejectButton.setButtonText("EJECT");
    ejectButton.setTooltip("Clear loaded MIDI and reset engine.");
    ejectButton.setColour(juce::TextButton::buttonColourId, cBackground);
    ejectButton.setColour(juce::TextButton::textColourOffId, cRed);
    ejectButton.setColour(juce::TextButton::textColourOnId, cRed);
    ejectButton.setColour(juce::TextButton::buttonOnColourId, cRed.withAlpha(0.2f));
    ejectButton.onClick = [this] {
        engine.loadSource(juce::File()); // Clearing the engine
        logMessage("DATA CLEARED.");
        repaint();
        };

    // --- Footer Actions ---
    auto setupBtn = [&](juce::TextButton& b, juce::Colour c, const juce::String& tip) {
        addAndMakeVisible(b);
        b.setColour(juce::TextButton::buttonColourId, cBackground);
        b.setColour(juce::TextButton::textColourOffId, c);
        b.setColour(juce::TextButton::buttonOnColourId, c);
        b.setColour(juce::TextButton::textColourOnId, cBackground);
        b.setTooltip(tip);
        };

    setupBtn(solveButton, cCyan, "Calculate the missing parameter based on current Mode.");
    solveButton.onClick = [this] { runSolver(); };

    setupBtn(generateButton, cGreen, "Generate the MIDI sequence in memory.");
    generateButton.setEnabled(false);
    generateButton.onClick = [this] { generate(); };

    setupBtn(saveButton, cOrange, "Write the generated MIDI file to disk.");
    saveButton.setEnabled(false);
    saveButton.onClick = [this] { saveFile(); };

    addAndMakeVisible(debugDumpToggle);
    debugDumpToggle.setColour(juce::ToggleButton::textColourId, cFrame);
    debugDumpToggle.setTooltip("Export a debug .txt file alongside the MIDI.");

    setSize(700, 600);
    updateInputStates();
}

MainComponent::~MainComponent() {}

// --- Drawing Helpers ---

void MainComponent::drawChamferedPanel(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& title, juce::Colour color)
{
    float c = 10.0f; // Chamfer amount

    juce::Path p;
    p.startNewSubPath(bounds.getX() + c, bounds.getY());
    p.lineTo(bounds.getRight() - c, bounds.getY());
    p.lineTo(bounds.getRight(), bounds.getY() + c);
    p.lineTo(bounds.getRight(), bounds.getBottom() - c);
    p.lineTo(bounds.getRight() - c, bounds.getBottom());
    p.lineTo(bounds.getX() + c, bounds.getBottom());
    p.lineTo(bounds.getX(), bounds.getBottom() - c);
    p.lineTo(bounds.getX(), bounds.getY() + c);
    p.closeSubPath();

    g.setColour(color.withAlpha(0.05f));
    g.fillPath(p);

    g.setColour(color.withAlpha(0.4f));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    if (title.isNotEmpty()) {
        auto titleArea = bounds.removeFromTop(20).reduced(10, 0);
        g.setColour(color);
        g.setFont(labelFont);
        g.drawText(title, titleArea, juce::Justification::left, false);

        g.setColour(color.withAlpha(0.3f));
        g.drawLine(bounds.getX(), bounds.getY() + 20, bounds.getRight(), bounds.getY() + 20, 1.0f);
    }
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(cBackground);

    // CRT Scanline effect
    g.setColour(juce::Colours::white.withAlpha(0.02f));
    for (int y = 0; y < getHeight(); y += 4)
        g.fillRect(0, y, getWidth(), 1);

    auto area = getLocalBounds().toFloat().reduced(15);

    // Header
    auto header = area.removeFromTop(30);
    g.setFont(headerFont);
    g.setColour(juce::Colours::white);
    g.drawText("CYCLESNAP v1.0", header, juce::Justification::topLeft, true);

    g.setColour(cFrame);
    g.fillRect(header.getRight() - 100, header.getY() + 10, 100.0f, 10.0f);

    area.removeFromTop(10);
    auto footer = area.removeFromBottom(50);

    // Grid Layout
    auto topRow = area.removeFromTop(area.getHeight() * 0.35f);
    area.removeFromTop(10);
    auto botRow = area;

    // --- Panel: Data Port ---
    auto modA = topRow.removeFromLeft(topRow.getWidth() * 0.4f);
    topRow.removeFromLeft(10);
    drawChamferedPanel(g, modA, "DATA_PORT", cCyan);

    // Data Port Internal Drawing
    if (modA.getHeight() > 50) {
        auto inner = modA.reduced(10);
        inner.removeFromTop(20);

        if (engine.isSourceLoaded()) {
            g.setColour(cCyan);
            g.setFont(dataFont);
            g.drawText("MEDIA LOADED", inner.removeFromTop(20), juce::Justification::centred, true);

            g.setFont(labelFont);
            g.setColour(cGreen);

            juce::String stats = "TRK: " + juce::String(engine.getSourceTrackCount()) + "\n" +
                "BPM: " + juce::String(engine.getSourceBPM());
            g.drawText(stats, inner, juce::Justification::centred, true);
        }
        else {
            // Drop Zone visuals
            float dashLen[] = { 4.0f, 4.0f };
            juce::Path p;
            p.addRectangle(inner.toFloat());

            juce::Path dashedPath;
            juce::PathStrokeType(2.0f).createDashedStroke(dashedPath, p, dashLen, 2);

            g.setColour(cFrame);
            g.fillPath(dashedPath);

            if (isDragActive) {
                g.setColour(cGreen);
                g.drawText(">> DROP HERE <<", inner, juce::Justification::centred, true);
            }
            else {
                g.setColour(cFrame);
                g.drawText("DROP MIDI HERE", inner, juce::Justification::centred, true);
            }
        }
    }

    // --- Panel: Visualizer ---
    auto modC = topRow;
    drawChamferedPanel(g, modC, "VISUALIZER", cGreen);

    // Placeholder content
    g.setColour(cFrame.darker(0.5f));
    g.drawRect(modC.reduced(20).toFloat(), 1.0f);
    g.setColour(cFrame);
    g.drawText("[ OFFLINE ]", modC, juce::Justification::centred, true);

    // --- Panel: Control Core ---
    auto modB = botRow.removeFromLeft(botRow.getWidth() * 0.5f);
    botRow.removeFromLeft(10);
    drawChamferedPanel(g, modB, "CONTROL_CORE", cOrange);

    // --- Panel: System Log ---
    auto modD = botRow;
    drawChamferedPanel(g, modD, "SYSTEM_LOG", cGreen);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced(15);
    area.removeFromTop(40);
    auto footer = area.removeFromBottom(50);

    auto topRow = area.removeFromTop(area.getHeight() * 0.35f);
    area.removeFromTop(10);
    auto botRow = area;

    // Data Port
    auto modA = topRow.removeFromLeft(topRow.getWidth() * 0.4f);
    loadButton.setBounds(modA.toNearestInt());

    int btnSize = 60;
    int btnH = 20;
    ejectButton.setBounds(modA.getX() + modA.getWidth() - btnSize - 10,
        modA.getY() + 25,
        btnSize, btnH);

    // Controls
    auto modB = botRow.removeFromLeft(botRow.getWidth() * 0.5f);
    modB.reduce(15, 15);
    modB.removeFromTop(15);

    modeSelector.setBounds(modB.removeFromTop(25));
    modB.removeFromTop(10);
    chkIntLoops.setBounds(modB.removeFromTop(20));
    modB.removeFromTop(15);

    auto grid = modB;
    int rowH = 50;

    auto r1 = grid.removeFromTop(rowH);
    auto c1 = r1.removeFromLeft(r1.getWidth() / 2 - 5);
    lblN.setBounds(c1.removeFromTop(20)); inputN.setBounds(c1);
    r1.removeFromLeft(10);
    lblS.setBounds(r1.removeFromTop(20)); inputS.setBounds(r1);

    grid.removeFromTop(10);

    auto r2 = grid.removeFromTop(rowH);
    auto c3 = r2.removeFromLeft(r2.getWidth() / 2 - 5);
    lblR.setBounds(c3.removeFromTop(20)); inputR.setBounds(c3);
    r2.removeFromLeft(10);
    lblSend.setBounds(r2.removeFromTop(20)); inputSend.setBounds(r2);

    // Logs
    auto modD = botRow;
    modD.reduce(15, 15);
    modD.removeFromTop(5);

    lblErrorMonitor.setBounds(modD.removeFromTop(20).removeFromRight(100));
    modD.removeFromTop(5);
    logEditor.setBounds(modD);

    // Footer Buttons
    auto btnArea = footer.reduced(20, 5);
    int btnW = 120;
    int gap = 10;

    solveButton.setBounds(btnArea.getX(), btnArea.getY(), btnW, 40);
    generateButton.setBounds(solveButton.getRight() + gap, btnArea.getY(), btnW, 40);
    saveButton.setBounds(generateButton.getRight() + gap, btnArea.getY(), btnW, 40);

    debugDumpToggle.setBounds(saveButton.getRight() + 20, btnArea.getY(), 100, 40);
}

// --- Logic Implementation ---

void MainComponent::updateInputStates()
{
    auto setFieldState = [&](juce::TextEditor& e, bool enabled) {
        e.setEnabled(enabled);
        e.setAlpha(enabled ? 1.0f : 0.3f);
        e.setColour(juce::TextEditor::outlineColourId, enabled ? cCyan : cFrame);
        };

    int modeId = modeSelector.getSelectedId();

    bool N_Enabled = (modeId == 1 || modeId == 2 || modeId == 3);
    bool S_Enabled = (modeId == 2 || modeId == 4);
    bool R_Enabled = (modeId == 1 || modeId == 4 || modeId == 5);
    bool End_Enabled = (modeId == 3 || modeId == 5);

    setFieldState(inputN, N_Enabled);
    setFieldState(inputS, S_Enabled);
    setFieldState(inputR, R_Enabled);
    setFieldState(inputSend, End_Enabled);
}

void MainComponent::updateErrorDisplay(double errorMs)
{
    juce::String text = "DRIFT: " + juce::String(errorMs, 2) + "ms";
    lblErrorMonitor.setText(text, juce::dontSendNotification);

    if (errorMs < 10.0)      lblErrorMonitor.setColour(juce::Label::textColourId, cGreen);
    else if (errorMs < 30.0) lblErrorMonitor.setColour(juce::Label::textColourId, cOrange);
    else                     lblErrorMonitor.setColour(juce::Label::textColourId, cRed);
}

void MainComponent::logMessage(const juce::String& msg)
{
    logEditor.moveCaretToEnd();
    logEditor.insertTextAtCaret(">> " + msg + "\n");
}

void MainComponent::loadFile(const juce::File& file) {
    logMessage("ACCESSING: " + file.getFileName());
    auto res = engine.loadSource(file);
    if (res.wasOk()) {
        logMessage("SOURCE LOADED.");
        generateButton.setEnabled(false);
        saveButton.setEnabled(false);
    }
    else {
        logMessage("ERROR: " + res.getErrorMessage());
    }
    repaint();
}

void MainComponent::runSolver() {
    if (!engine.isSourceLoaded()) { logMessage("ERROR: NO SOURCE."); return; }
    logMessage("CALCULATING...");

    double reps = inputN.getText().getDoubleValue();
    double s = inputS.getText().getDoubleValue();
    double R = inputR.getText().getDoubleValue();
    double end = inputSend.getText().getDoubleValue();
    bool integerLoops = chkIntLoops.getToggleState();

    auto mode = GeoTimeMath::Mode::TargetTotalScale;
    switch (modeSelector.getSelectedId()) {
    case 1: mode = GeoTimeMath::Mode::TargetTotalScale; break;
    case 2: mode = GeoTimeMath::Mode::FixedBeatRatio; break;
    case 3: mode = GeoTimeMath::Mode::MatchBeatEnd; break;
    case 4: mode = GeoTimeMath::Mode::FitToCurve; break;
    case 5: mode = GeoTimeMath::Mode::FitEndAndRatio; break;
    }

    auto result = engine.runSolver(mode, reps, s, R, end, integerLoops);

    if (result.success) {
        int M = engine.getSegmentCount(); if (M < 1) M = 1;
        double displayLoops = (double)result.repetitions / M;

        logMessage(juce::String("SOLVED: N=") + juce::String(result.repetitions) +
            " (" + juce::String(displayLoops, 2) + " Loops)");

        updateErrorDisplay(result.errorMs);

        // Feedback calculated values to inputs
        inputN.setText(juce::String(displayLoops, 2));
        inputS.setText(juce::String(result.beatRatio, 5));
        inputR.setText(juce::String(result.totalScale, 5));
        inputSend.setText(juce::String(result.beatEnd, 5));

        generateButton.setEnabled(true);
    }
    else {
        logMessage("MATH ERROR: " + juce::String(result.message));
        updateErrorDisplay(999.0);
    }
}

void MainComponent::generate() {
    double reps = inputN.getText().getDoubleValue();
    double s = inputS.getText().getDoubleValue();
    double R = inputR.getText().getDoubleValue();
    double end = inputSend.getText().getDoubleValue();
    bool integerLoops = chkIntLoops.getToggleState();

    auto mode = GeoTimeMath::Mode::TargetTotalScale;
    switch (modeSelector.getSelectedId()) {
    case 1: mode = GeoTimeMath::Mode::TargetTotalScale; break;
    case 2: mode = GeoTimeMath::Mode::FixedBeatRatio; break;
    case 3: mode = GeoTimeMath::Mode::MatchBeatEnd; break;
    case 4: mode = GeoTimeMath::Mode::FitToCurve; break;
    case 5: mode = GeoTimeMath::Mode::FitEndAndRatio; break;
    }

    auto res = engine.runSolver(mode, reps, s, R, end, integerLoops);

    if (res.success) {
        if (engine.generateOutput(res.repetitions, res.stepScale).wasOk()) {
            logMessage("SEQUENCE GENERATED.");
            saveButton.setEnabled(true);
        }
        else {
            logMessage("GEN FAIL: Engine Error.");
        }
    }
    else {
        logMessage("GEN FAIL: Invalid Parameters.");
    }
}

void MainComponent::saveFile() {
    auto fc = std::make_shared<juce::FileChooser>("SAVE OUTPUT",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.mid");

    fc->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser& chooser) {
            auto resultFile = chooser.getResult();
            if (resultFile != juce::File()) {
                if (engine.saveFile(resultFile).wasOk()) {
                    logMessage("SAVED: " + resultFile.getFileName());

                    if (debugDumpToggle.getToggleState()) {
                        resultFile.withFileExtension("txt")
                            .replaceWithText(engine.getDebugDump());
                        logMessage("DEBUG DUMP EXPORTED.");
                    }
                }
            }
        });
}

// --- Drag and Drop ---
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files) {
    return files.size() == 1 && files[0].endsWithIgnoreCase(".mid");
}
void MainComponent::fileDragEnter(const juce::StringArray&, int, int) {
    isDragActive = true; repaint();
}
void MainComponent::fileDragExit(const juce::StringArray&) {
    isDragActive = false; repaint();
}
void MainComponent::filesDropped(const juce::StringArray& files, int, int) {
    isDragActive = false; repaint();
    if (files.size() > 0) loadFile(juce::File(files[0]));
}
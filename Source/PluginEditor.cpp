#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FluidJustIntonationEditor::FluidJustIntonationEditor (FluidJustIntonationProcessor& p)
	: AudioProcessorEditor (&p), audioProcessor (p)
{
	// Apply custom look and feel
	setLookAndFeel(&customLookAndFeel);
	
	// Sequence length buttons - group them
	auto setupButton = [this](juce::TextButton& button, const juce::String& text, bool isSelected) {
		button.setButtonText(text);
		button.setToggleState(isSelected, juce::dontSendNotification);
		button.setRadioGroupId(1); // Make them work as a group
		addAndMakeVisible(button);
	};
	
	setupButton(sequenceLength4Button, "4", true);
	sequenceLength4Button.onClick = [this] { sequenceLengthChanged(4); };
	
	setupButton(sequenceLength8Button, "8", false);
	sequenceLength8Button.onClick = [this] { sequenceLengthChanged(8); };
	
	setupButton(sequenceLength12Button, "12", false);
	sequenceLength12Button.onClick = [this] { sequenceLengthChanged(12); };
	
	setupButton(sequenceLength16Button, "16", false);
	sequenceLength16Button.onClick = [this] { sequenceLengthChanged(16); };
	
	// Intonation mode buttons - group them
	setupButton(setModeButton, "Set Mode", true);
	setModeButton.setRadioGroupId(2); // Different group from sequence buttons
	setModeButton.onClick = [this] { intonationModeChanged(FluidJustIntonationProcessor::IntonationMode::Set); };
	
	setupButton(shiftModeButton, "Shift Mode", false);
	shiftModeButton.setRadioGroupId(2);
	shiftModeButton.onClick = [this] { intonationModeChanged(FluidJustIntonationProcessor::IntonationMode::Shift); };
	
	// Current state labels with consistent styling
	auto setupLabel = [this](juce::Label& label, const juce::String& text) {
		label.setText(text, juce::dontSendNotification);
		label.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 15.0f, juce::Font::bold));
		label.setJustificationType(juce::Justification::centredLeft);
		addAndMakeVisible(label);
	};
	
	setupLabel(currentMeasureLabel, "Current Measure: 1");
	setupLabel(currentRootLabel, "Current Root: C");
	setupLabel(currentModeLabel, "Mode: Set");
	
	// SoundFont UI setup
	loadSoundFontButton.onClick = [this] { loadSoundFontClicked(); };
	addAndMakeVisible(loadSoundFontButton);
	
	unloadSoundFontButton.onClick = [this] { unloadSoundFontClicked(); };
	unloadSoundFontButton.setEnabled(false);
	addAndMakeVisible(unloadSoundFontButton);
	
	sineWaveModeButton.onClick = [this] { synthModeChanged(FluidJustIntonationSynth::SynthMode::SineWave); };
	sineWaveModeButton.setToggleState(true, juce::dontSendNotification);
	sineWaveModeButton.setRadioGroupId(3);
	addAndMakeVisible(sineWaveModeButton);
	
	soundFontNameLabel.setText("No SoundFont loaded", juce::dontSendNotification);
	soundFontNameLabel.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::plain));
	soundFontNameLabel.setJustificationType(juce::Justification::centredLeft);
	soundFontNameLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.7f));
	addAndMakeVisible(soundFontNameLabel);
	
	// presetLabel.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::bold));
	// presetLabel.setJustificationType(juce::Justification::centredRight);
	// addAndMakeVisible(presetLabel);
	
	// presetSelector.onChange = [this] { presetChanged(); };
	// presetSelector.setEnabled(false);
	// addAndMakeVisible(presetSelector);
	
	// Set up the measure root selectors
	updateMeasureRootSelectors();
	
	// Update SoundFont UI state
	updateSoundFontUI();
	
	// Set the editor size (increased height for new controls)
	setSize(700, 650);
	
	// Start timer for UI updates
	startTimerHz(30); // Update 30 times per second
}

FluidJustIntonationEditor::~FluidJustIntonationEditor()
{
	// Clean up look and feel to avoid dangling pointers
	setLookAndFeel(nullptr);
	
	// Stop timer
	stopTimer();
}

//==============================================================================
void FluidJustIntonationEditor::paint(juce::Graphics& g)
{
	// Fill the background
	g.fillAll(backgroundColour);
	
	// Draw title with gradient background
	auto titleArea = getLocalBounds().removeFromTop(60);
	
	juce::ColourGradient titleGradient(
		backgroundColour.brighter(0.1f), 0, titleArea.getY(),
		backgroundColour.darker(0.1f), 0, titleArea.getBottom(),
		false);
	g.setGradientFill(titleGradient);
	g.fillRect(titleArea);
	
	g.setColour(accentColour);
	g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 28.0f, juce::Font::bold));
	g.drawText("Fluid Just Intonation", titleArea, juce::Justification::centred, true);
	
	// Draw section backgrounds and titles
	auto drawSection = [&](juce::Rectangle<int>& area, const juce::String& title) {
		// Rounded panel background
		g.setColour(backgroundColour.brighter(0.05f));
		auto sectionBounds = area.reduced(5).toFloat();
		g.fillRoundedRectangle(sectionBounds, 5.0f);
		
		// Draw title
		g.setColour(textColour);
		g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 18.0f, juce::Font::bold));
		g.drawText(title, area.removeFromTop(25).reduced(10, 0), 
				  juce::Justification::centredLeft, true);
		
		// Draw subtle border
		g.setColour(highlightColour.withAlpha(0.3f));
		g.drawRoundedRectangle(sectionBounds, 5.0f, 1.0f);
	};
	
	// Layout main areas
	auto mainArea = getLocalBounds().reduced(10).withTop(titleArea.getBottom());
	
	// SoundFont section at the top
	auto soundFontArea = mainArea.removeFromTop(100);
	drawSection(soundFontArea, "Sound Source");
	
	auto topArea = mainArea.removeFromTop(140);
	auto leftArea = topArea.removeFromLeft(300);
	auto rightArea = topArea;
	
	// Draw sections
	drawSection(leftArea, "Sequence Settings");
	drawSection(rightArea, "Current Status");
	
	auto measuresArea = mainArea.removeFromTop(160);
	drawSection(measuresArea, "Measure Roots");
	
	auto visualizationArea = mainArea.removeFromTop(140);
	
	// Draw frequency display in its own section
	drawFrequencyDisplay(g, visualizationArea.reduced(10));
}

void FluidJustIntonationEditor::resized()
{
	// Main layout areas
	auto mainArea = getLocalBounds().reduced(10).withTop(60); // Title takes top 60px
	
	// SoundFont section
	auto soundFontArea = mainArea.removeFromTop(100);
	soundFontArea.removeFromTop(30); // Account for section title
	soundFontArea = soundFontArea.reduced(15, 5);
	
	// First row: mode buttons and load button
	auto sfRow1 = soundFontArea.removeFromTop(35);
	sineWaveModeButton.setBounds(sfRow1.removeFromLeft(100));
	sfRow1.removeFromLeft(10);
	loadSoundFontButton.setBounds(sfRow1.removeFromLeft(120));
	sfRow1.removeFromLeft(10);
	unloadSoundFontButton.setBounds(sfRow1.removeFromLeft(80));
	sfRow1.removeFromLeft(10);
	soundFontNameLabel.setBounds(sfRow1);
	
	// Second row: preset selector
	// auto sfRow2 = soundFontArea.removeFromTop(35);
	// presetLabel.setBounds(sfRow2.removeFromLeft(60));
	// sfRow2.removeFromLeft(10);
	// presetSelector.setBounds(sfRow2.removeFromLeft(300));
	
	// Rest of the layout
	auto topArea = mainArea.removeFromTop(160);
	auto leftArea = topArea.removeFromLeft(300);
	auto rightArea = topArea;
	
	// Account for section titles by removing 25px from top
	leftArea.removeFromTop(25);
	rightArea.removeFromTop(25);
	
	// Layout sequence length buttons with even spacing
	auto sequenceLengthArea = leftArea.removeFromTop(40).reduced(10, 0);
	juce::FlexBox sequenceFlexBox;
	sequenceFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
	sequenceFlexBox.alignItems = juce::FlexBox::AlignItems::center;
	
	sequenceFlexBox.items.add(juce::FlexItem(60, 30, sequenceLength4Button));
	sequenceFlexBox.items.add(juce::FlexItem(60, 30, sequenceLength8Button));
	sequenceFlexBox.items.add(juce::FlexItem(60, 30, sequenceLength12Button));
	sequenceFlexBox.items.add(juce::FlexItem(60, 30, sequenceLength16Button));
	
	sequenceFlexBox.performLayout(sequenceLengthArea);
	
	// Layout intonation mode buttons
	auto intonationModeArea = leftArea.removeFromTop(40).reduced(10, 0);
	juce::FlexBox modeFlexBox;
	modeFlexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
	modeFlexBox.alignItems = juce::FlexBox::AlignItems::center;
	
	modeFlexBox.items.add(juce::FlexItem(130, 30, setModeButton));
	modeFlexBox.items.add(juce::FlexItem(130, 30, shiftModeButton));
	
	modeFlexBox.performLayout(intonationModeArea);
	
	// Layout current status labels with good spacing
	auto statusArea = rightArea.reduced(10, 0);
	currentMeasureLabel.setBounds(statusArea.removeFromTop(30));
	statusArea.removeFromTop(5); // Add spacing
	currentRootLabel.setBounds(statusArea.removeFromTop(30));
	statusArea.removeFromTop(5); // Add spacing
	currentModeLabel.setBounds(statusArea.removeFromTop(30));
	
	// Layout measure root selectors in a grid
	auto measureRootsArea = mainArea.removeFromTop(180);
	measureRootsArea.removeFromTop(25); // Space for the section title
	
	auto selectorArea = measureRootsArea.reduced(10, 5);
	
	// Calculate optimal layout based on available space and number of selectors
	int totalSelectors = static_cast<int>(measureRootSelectors.size());
	int maxSelectorsPerRow = juce::jmin(8, totalSelectors); // Don't have too many per row
	
	// Calculate rows needed
	int rows = (totalSelectors + maxSelectorsPerRow - 1) / maxSelectorsPerRow;
	
	// Adjust selectors per row for more even distribution
	int selectorsPerRow = (totalSelectors + rows - 1) / rows;
	
	// Calculate selector dimensions with appropriate spacing
	int hspacing = 10;
	int vspacing = 10;
	int selectorWidth = (selectorArea.getWidth() - (selectorsPerRow - 1) * hspacing) / selectorsPerRow;
	int selectorHeight = 30; // Fixed height for consistency
	
	// Position each selector
	for (int i = 0; i < totalSelectors; ++i)
	{
		int row = i / selectorsPerRow;
		int col = i % selectorsPerRow;
		
		int x = selectorArea.getX() + col * (selectorWidth + hspacing);
		int y = selectorArea.getY() + row * (selectorHeight + vspacing);
		
		measureRootSelectors[i]->setBounds(x, y, selectorWidth, selectorHeight);
	}
	
	// Leave the piano roll area to be handled in the paint method
}

//==============================================================================
void FluidJustIntonationEditor::timerCallback()
{
	// Update status labels
	int currentMeasure = 0;
	juce::AudioPlayHead::CurrentPositionInfo posInfo;
	if (auto* playHead = audioProcessor.getPlayHead())
	{
		auto position = playHead->getPosition();
		if (position && position->getPpqPosition().hasValue())
		{
			currentMeasure = static_cast<int>(position->getPpqPosition().orFallback(0.0) / 4.0);
		}
	}
	currentMeasure = currentMeasure % audioProcessor.getSequenceLength();
	
	currentMeasureLabel.setText("Current Measure: " + juce::String(currentMeasure + 1), 
								juce::dontSendNotification);
	
	int currentRoot = audioProcessor.getMeasureRoot(currentMeasure);
	juce::String rootName;
	
	switch (currentRoot % 12)
	{
		case 0: rootName = "C"; break;
		case 1: rootName = "C#"; break;
		case 2: rootName = "D"; break;
		case 3: rootName = "D#"; break;
		case 4: rootName = "E"; break;
		case 5: rootName = "F"; break;
		case 6: rootName = "F#"; break;
		case 7: rootName = "G"; break;
		case 8: rootName = "G#"; break;
		case 9: rootName = "A"; break;
		case 10: rootName = "A#"; break;
		case 11: rootName = "B"; break;
	}
	
	currentRootLabel.setText("Current Root: " + rootName, juce::dontSendNotification);
	
	// Highlight current measure selector
	for (int i = 0; i < measureRootSelectors.size(); ++i)
	{
		if (i == currentMeasure)
		{
			measureRootSelectors[i]->setColour(juce::ComboBox::backgroundColourId, highlightColour);
			measureRootSelectors[i]->setColour(juce::ComboBox::outlineColourId, accentColour);
		}
		else
		{
			measureRootSelectors[i]->setColour(juce::ComboBox::backgroundColourId, 
											 juce::Colour(0xff313244)); // Reset to default
			measureRootSelectors[i]->setColour(juce::ComboBox::outlineColourId, 
											 juce::Colours::grey.withAlpha(0.5f));
		}
	}
	
	// Trigger a repaint to update frequency display
	repaint();
}

void FluidJustIntonationEditor::sequenceLengthChanged(int newLength)
{
	// Update UI buttons
	sequenceLength4Button.setToggleState(newLength == 4, juce::dontSendNotification);
	sequenceLength8Button.setToggleState(newLength == 8, juce::dontSendNotification);
	sequenceLength12Button.setToggleState(newLength == 12, juce::dontSendNotification);
	sequenceLength16Button.setToggleState(newLength == 16, juce::dontSendNotification);
	
	// Update processor
	audioProcessor.setSequenceLength(newLength);
	
	// Update UI with new measure selectors
	updateMeasureRootSelectors();
	
	// Trigger a resize to layout new components
	resized();
}

void FluidJustIntonationEditor::intonationModeChanged(FluidJustIntonationProcessor::IntonationMode newMode)
{
	// Update UI buttons
	setModeButton.setToggleState(newMode == FluidJustIntonationProcessor::IntonationMode::Set, 
							   juce::dontSendNotification);
	shiftModeButton.setToggleState(newMode == FluidJustIntonationProcessor::IntonationMode::Shift, 
								 juce::dontSendNotification);
	
	// Update processor
	audioProcessor.setIntonationMode(newMode);
	
	// Update mode label
	if (newMode == FluidJustIntonationProcessor::IntonationMode::Set)
		currentModeLabel.setText("Mode: Set", juce::dontSendNotification);
	else
		currentModeLabel.setText("Mode: Shift", juce::dontSendNotification);
}

void FluidJustIntonationEditor::measureRootChanged(int measureIndex, int newRoot)
{
	// Update processor
	audioProcessor.setMeasureRoot(measureIndex, newRoot);
}

void FluidJustIntonationEditor::updateMeasureRootSelectors()
{
	// Clear existing selectors
	measureRootSelectors.clear();
	
	// Get current sequence length
	int length = audioProcessor.getSequenceLength();
	
	// Create new selectors
	for (int i = 0; i < length; ++i)
	{
		auto selector = std::make_unique<juce::ComboBox>("Measure " + juce::String(i + 1));
		
		// Add note options
		selector->addItem("C", 1);
		selector->addItem("C#", 2);
		selector->addItem("D", 3);
		selector->addItem("D#", 4);
		selector->addItem("E", 5);
		selector->addItem("F", 6);
		selector->addItem("F#", 7);
		selector->addItem("G", 8);
		selector->addItem("G#", 9);
		selector->addItem("A", 10);
		selector->addItem("A#", 11);
		selector->addItem("B", 12);
		
		// Set current value
		int currentRoot = audioProcessor.getMeasureRoot(i);
		selector->setSelectedId((currentRoot % 12) + 1);
		
		// Set callback
		selector->onChange = [this, i] {
			int selectedId = measureRootSelectors[i]->getSelectedId();
			if (selectedId > 0)
			{
				// Convert from 1-based ComboBox ID to 0-based note index
				int noteIndex = selectedId - 1;
				// Convert to MIDI note number (C4 = 60)
				int midiNote = 60 + noteIndex;
				measureRootChanged(i, midiNote);
			}
		};
		
		addAndMakeVisible(*selector);
		measureRootSelectors.push_back(std::move(selector));
	}
}

//==============================================================================
// SoundFont UI handlers

void FluidJustIntonationEditor::loadSoundFontClicked()
{
	fileChooser = std::make_unique<juce::FileChooser>(
		"Select a SoundFont file...",
		juce::File::getSpecialLocation(juce::File::userHomeDirectory),
		"*.sf2;*.SF2"
	);
	
	auto folderChooserFlags = juce::FileBrowserComponent::openMode | 
							  juce::FileBrowserComponent::canSelectFiles;
	
	fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc) {
		auto file = fc.getResult();
		
		if (file.existsAsFile())
		{
			if (audioProcessor.loadSoundFont(file))
			{
				updateSoundFontUI();
				updatePresetList();
			}
			else
			{
				juce::AlertWindow::showMessageBoxAsync(
					juce::MessageBoxIconType::WarningIcon,
					"SoundFont Error",
					"Failed to load SoundFont file: " + file.getFileName()
				);
			}
		}
	});
}

void FluidJustIntonationEditor::unloadSoundFontClicked()
{
	audioProcessor.unloadSoundFont();
	updateSoundFontUI();
}

void FluidJustIntonationEditor::presetChanged()
{
	int selectedPreset = presetSelector.getSelectedId() - 1; // ComboBox IDs are 1-based
	if (selectedPreset >= 0)
	{
		audioProcessor.setPreset(selectedPreset);
	}
}

void FluidJustIntonationEditor::synthModeChanged(FluidJustIntonationSynth::SynthMode mode)
{
	audioProcessor.setSynthMode(mode);
	sineWaveModeButton.setToggleState(mode == FluidJustIntonationSynth::SynthMode::SineWave, 
									  juce::dontSendNotification);
}

void FluidJustIntonationEditor::updateSoundFontUI()
{
	bool loaded = audioProcessor.isSoundFontLoaded();
	
	unloadSoundFontButton.setEnabled(loaded);
	// presetSelector.setEnabled(loaded);
	
	if (loaded)
	{
		soundFontNameLabel.setText(audioProcessor.getSoundFontName(), juce::dontSendNotification);
		soundFontNameLabel.setColour(juce::Label::textColourId, successColour);
		
		// Auto-switch to SoundFont mode
		sineWaveModeButton.setToggleState(false, juce::dontSendNotification);
	}
	else
	{
		soundFontNameLabel.setText("No SoundFont loaded", juce::dontSendNotification);
		soundFontNameLabel.setColour(juce::Label::textColourId, textColour.withAlpha(0.7f));
		presetSelector.clear();
		
		// Switch back to sine wave mode
		sineWaveModeButton.setToggleState(true, juce::dontSendNotification);
	}
}

void FluidJustIntonationEditor::updatePresetList()
{
	// presetSelector.clear();
	
	int presetCount = audioProcessor.getPresetCount();
	
	for (int i = 0; i < presetCount; ++i)
	{
		juce::String presetName = audioProcessor.getPresetName(i);
		if (presetName.isEmpty())
			presetName = "Preset " + juce::String(i);
		
		presetSelector.addItem(juce::String(i) + ": " + presetName, i + 1);
	}
	
	// Select the current preset
	int currentPreset = audioProcessor.getCurrentPreset();
	presetSelector.setSelectedId(currentPreset + 1, juce::dontSendNotification);
}

//==============================================================================

void FluidJustIntonationEditor::drawFrequencyDisplay(juce::Graphics& g, juce::Rectangle<int> area)
{
	g.setColour(backgroundColour.darker(0.1f));
	g.fillRoundedRectangle(area.toFloat(), 5.0f);
	
	// Draw title
	g.setColour(textColour);
	g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 18.0f, juce::Font::bold));
	auto titleArea = area.removeFromTop(25);
	g.drawText("Current Tuning (C5-B5)", titleArea, juce::Justification::centred, true);
	
	// Note names for display
	const juce::String noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
	
	// C5 is MIDI note 72, B5 is MIDI note 83
	const int startNote = 72; // C5
	const int numNotes = 12;
	
	// Get current root for highlighting
	int currentRoot = audioProcessor.getMeasureRoot(audioProcessor.getCurrentMeasure());
	int rootNoteIndex = currentRoot % 12;
	
	// Calculate layout
	float noteWidth = area.getWidth() / static_cast<float>(numNotes);
	float noteHeight = area.getHeight() - 10;
	
	for (int i = 0; i < numNotes; ++i)
	{
		int midiNote = startNote + i;
		int noteIndex = i; // 0 = C, 1 = C#, etc.
		
		// Get the frequency for this note from the processor
		double frequency = audioProcessor.getFrequencyForNote(midiNote);
		
		// Calculate position
		float x = area.getX() + i * noteWidth;
		float y = area.getY() + 5;
		
		// Determine if this is a "black key" note (for coloring)
		bool isSharp = (noteIndex == 1 || noteIndex == 3 || noteIndex == 6 || 
					   noteIndex == 8 || noteIndex == 10);
		
		// Determine if this note is the root
		bool isRoot = (noteIndex == rootNoteIndex);
		
		// Draw background box
		juce::Rectangle<float> noteBox(x + 2, y, noteWidth - 4, noteHeight);
		
		if (isRoot)
		{
			g.setColour(accentColour.withAlpha(0.3f));
		}
		else if (isSharp)
		{
			g.setColour(backgroundColour.darker(0.2f));
		}
		else
		{
			g.setColour(backgroundColour.brighter(0.05f));
		}
		
		g.fillRoundedRectangle(noteBox, 3.0f);
		
		// Draw border
		if (isRoot)
		{
			g.setColour(accentColour);
			g.drawRoundedRectangle(noteBox, 3.0f, 2.0f);
		}
		else
		{
			g.setColour(highlightColour.withAlpha(0.2f));
			g.drawRoundedRectangle(noteBox, 3.0f, 1.0f);
		}
		
		// Draw note name
		g.setColour(isRoot ? accentColour : textColour);
		g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 14.0f, juce::Font::bold));
		g.drawText(noteNames[noteIndex] + "5", noteBox.removeFromTop(22), 
				   juce::Justification::centred, false);
		
		// Draw frequency
		g.setColour(isRoot ? accentColour : textColour.withAlpha(0.8f));
		g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::plain));
		
		// Format frequency nicely
		juce::String freqStr;
		if (frequency >= 1000.0)
		{
			freqStr = juce::String(frequency, 1) + " Hz";
		}
		else
		{
			freqStr = juce::String(frequency, 2) + " Hz";
		}
		
		g.drawText(freqStr, noteBox.removeFromTop(20), juce::Justification::centred, false);
		
		// Draw cents deviation from 12-TET
		double tetFreq = 440.0 * std::pow(2.0, (midiNote - 69) / 12.0);
		double cents = 1200.0 * std::log2(frequency / tetFreq);
		
		g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::plain));
		
		if (std::abs(cents) < 0.5)
		{
			g.setColour(successColour.withAlpha(0.7f));
			g.drawText("0c", noteBox, juce::Justification::centred, false);
		}
		else
		{
			// Color based on deviation magnitude
			if (std::abs(cents) > 50)
				g.setColour(juce::Colour(0xfff38ba8)); // Red for large deviation
			else if (std::abs(cents) > 20)
				g.setColour(juce::Colour(0xfffab387)); // Orange for medium
			else
				g.setColour(successColour.withAlpha(0.7f)); // Green for small
			
			juce::String centsStr = (cents > 0 ? "+" : "") + juce::String(cents, 1) + "c";
			g.drawText(centsStr, noteBox, juce::Justification::centred, false);
		}
	}
}

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * FluidJustIntonationEditor - The custom GUI for the Fluid Just Intonation VST
 */

class FluidJustLookAndFeel : public juce::LookAndFeel_V4
{
public:
	FluidJustLookAndFeel()
	{
		// Define a color palette
		setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff1e1e2e));  // Dark background
		setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff313244));       // Slightly lighter background
		setColour(juce::ComboBox::textColourId, juce::Colour(0xffcdd6f4));            // Light text
		setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff89b4fa));           // Light blue for UI elements
		
		setColour(juce::TextButton::buttonColourId, juce::Colour(0xff313244));         // Button background
		setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff89b4fa));      // Active button color
		setColour(juce::TextButton::textColourOffId, juce::Colour(0xffcdd6f4));       // Button text
		setColour(juce::TextButton::textColourOnId, juce::Colour(0xff1e1e2e));        // Active button text
		
		setColour(juce::Label::textColourId, juce::Colour(0xffcdd6f4));               // Label text
		
		// Set default font
		setDefaultSansSerifTypefaceName("Arial");
	}
	
	void drawButtonBackground(juce::Graphics& g, juce::Button& button, 
							 const juce::Colour& backgroundColour,
							 bool shouldDrawButtonAsHighlighted, 
							 bool shouldDrawButtonAsDown) override
	{
		// Create rounded rectangles for the buttons
		auto buttonArea = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);
		float cornerSize = 4.0f;
		
		// Choose the right color based on button state
		juce::Colour baseColour = backgroundColour;
		
		if (button.getToggleState())
			baseColour = findColour(juce::TextButton::buttonOnColourId);
			
		if (shouldDrawButtonAsDown)
			baseColour = baseColour.darker(0.1f);
		else if (shouldDrawButtonAsHighlighted)
			baseColour = baseColour.brighter(0.1f);
		
		// Draw button with gradient
		g.setGradientFill(juce::ColourGradient(baseColour.brighter(0.05f), 0.0f, 0.0f,
											  baseColour.darker(0.05f), 0.0f, buttonArea.getHeight(),
											  false));
		g.fillRoundedRectangle(buttonArea, cornerSize);
		
		// Draw outline
		g.setColour(button.findColour(button.getToggleState() ? 
									 juce::TextButton::textColourOnId : 
									 juce::TextButton::textColourOffId).withAlpha(0.4f));
		g.drawRoundedRectangle(buttonArea, cornerSize, 1.0f);
	}
	
	void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
					 int buttonX, int buttonY, int buttonW, int buttonH,
					 juce::ComboBox& box) override
	{
		// Rounded rectangle for combo boxes
		auto boxBounds = juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f, 0.5f);
		float cornerSize = 4.0f;
		
		g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
		g.fillRoundedRectangle(boxBounds, cornerSize);
		
		// Draw outline
		g.setColour(box.findColour(juce::ComboBox::outlineColourId).withAlpha(0.5f));
		g.drawRoundedRectangle(boxBounds, cornerSize, 1.0f);
		
		// Draw the arrow
		auto arrowZone = juce::Rectangle<int>(width - 20, 0, 20, height).toFloat();
		g.setColour(box.findColour(juce::ComboBox::arrowColourId));
		auto arrowSize = juce::jmin(12.0f, box.getHeight() * 0.3f);
		
		g.drawLine(arrowZone.getCentreX() - arrowSize, arrowZone.getCentreY() - arrowSize/2,
				  arrowZone.getCentreX(), arrowZone.getCentreY() + arrowSize/2);
		g.drawLine(arrowZone.getCentreX() + arrowSize, arrowZone.getCentreY() - arrowSize/2,
				  arrowZone.getCentreX(), arrowZone.getCentreY() + arrowSize/2);
	}
};

class FluidJustIntonationEditor  : public juce::AudioProcessorEditor,
								  private juce::Timer
{
public:
	FluidJustIntonationEditor (FluidJustIntonationProcessor&);
	~FluidJustIntonationEditor() override;

	//==============================================================================
	void paint (juce::Graphics&) override;
	void resized() override;

private:
	// This reference is provided as a quick way for your editor to
	// access the processor object that created it.
	FluidJustIntonationProcessor& audioProcessor;
	
	// Custom look and feel
	FluidJustLookAndFeel customLookAndFeel;
	
	// Color palette
	const juce::Colour backgroundColour = juce::Colour(0xff1e1e2e);  // Dark background
	const juce::Colour highlightColour = juce::Colour(0xff89b4fa);   // Light blue highlight
	const juce::Colour accentColour = juce::Colour(0xffcba6f7);      // Purple accent
	const juce::Colour textColour = juce::Colour(0xffcdd6f4);        // Light text
	const juce::Colour successColour = juce::Colour(0xffa6e3a1);     // Green for success
	
	// Draw the frequency display (replaces piano roll)
	void drawFrequencyDisplay(juce::Graphics& g, juce::Rectangle<int> area);
	
	// UI Components - Sequence Settings
	juce::TextButton sequenceLength4Button;
	juce::TextButton sequenceLength8Button;
	juce::TextButton sequenceLength12Button;
	juce::TextButton sequenceLength16Button;
	
	juce::TextButton setModeButton;
	juce::TextButton shiftModeButton;
	
	// Combo boxes for note selection in each measure
	std::vector<std::unique_ptr<juce::ComboBox>> measureRootSelectors;
	
	// Current state display
	juce::Label currentMeasureLabel;
	juce::Label currentRootLabel;
	juce::Label currentModeLabel;
	
	// SoundFont UI components
	juce::TextButton loadSoundFontButton { "Load SoundFont" };
	juce::TextButton unloadSoundFontButton { "Unload" };
	juce::TextButton sineWaveModeButton { "Sine Wave" };
	juce::Label soundFontNameLabel;
	juce::ComboBox presetSelector;
	juce::Label presetLabel { {}, "Preset:" };
	
	// Visualization of the just intonation scale
	juce::DrawableRectangle pianoRoll;
	
	// Timer callback for UI updates
	void timerCallback() override;
	
	// UI event handlers
	void sequenceLengthChanged(int newLength);
	void intonationModeChanged(FluidJustIntonationProcessor::IntonationMode newMode);
	void measureRootChanged(int measureIndex, int newRoot);
	
	// SoundFont event handlers
	void loadSoundFontClicked();
	void unloadSoundFontClicked();
	void presetChanged();
	void synthModeChanged(FluidJustIntonationSynth::SynthMode mode);
	
	// Update the UI based on current sequence length
	void updateMeasureRootSelectors();
	
	// Update SoundFont UI state
	void updateSoundFontUI();
	void updatePresetList();
	
	// File chooser for soundfont loading
	std::unique_ptr<juce::FileChooser> fileChooser;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FluidJustIntonationEditor)
};
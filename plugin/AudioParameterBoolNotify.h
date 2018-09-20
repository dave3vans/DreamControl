#pragma once

#include <functional>
#include "JuceLibraryCode/JuceHeader.h"

class AudioParameterBoolNotify : public AudioParameterBool
{
public:
	AudioParameterBoolNotify(const String& parameterID, const String& name, bool defaultValue,
		std::function<void(const String& paramID, bool newValue)> valueChangedFunction,
		const String& label = String(),
		std::function<String(bool value, int maximumStringLength)> stringFromBool = nullptr,
		std::function<bool(const String& text)> boolFromString = nullptr);

	void valueChanged(bool newValue) override;

private:
	std::function<void(const String&, bool)> myValueChangedFunction;
};
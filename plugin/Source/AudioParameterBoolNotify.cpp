#include <functional>
#include "AudioParameterBoolNotify.h"

AudioParameterBoolNotify::AudioParameterBoolNotify(const String& parameterID, const String& name, bool defaultValue,
	std::function<void(const String& paramId, bool newValue)> valueChangedFunction,
	const String& label,
	std::function<String(bool value, int maximumStringLength)> stringFromBool,
	std::function<bool(const String& text)> boolFromString)
	: AudioParameterBool(parameterID, name, defaultValue, label, stringFromBool, boolFromString)
{
	myValueChangedFunction = valueChangedFunction;
}

void AudioParameterBoolNotify::valueChanged(bool newValue)
{
	myValueChangedFunction(paramID, newValue);
}

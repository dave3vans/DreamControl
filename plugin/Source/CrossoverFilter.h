/*
  This code accompanies the textbook:

  Digital Audio Effects: Theory, Implementation and Application
  Joshua D. Reiss and Andrew P. McPherson

  ---

  Parametric EQ: parametric equaliser adjusting frequency, Q and gain
  See textbook Chapter 4: Filter Effects

  Code by Andrew McPherson, Brecht De Man and Joshua Reiss

  ---

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __PARAMETRICEQFILTER_H_6E48F605__
#define __PARAMETRICEQFILTER_H_6E48F605__

#define _USE_MATH_DEFINES
#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
/**
 * This class implements a biquad parametric EQ section according
 * to the equations in the Reiss and McPherson text.
 */

class CrossoverFilter : public IIRFilter
{
public:
    //==============================================================================

    CrossoverFilter(bool highpass, bool linkwitzRiley);
    ~CrossoverFilter() {}
    void makeCrossover (
        const double crossoverFrequency, 
        const int sampleRate, 
        const bool linkwitzRiley, 
        const bool highpass
    ) noexcept;

    void applyFilter(float* const samples, float* const output, const int numSamples) noexcept;


    /** Makes this filter duplicate the set-up of another one.
     */
    //void copyCoefficientsFrom (const CrossoverFilter& other) noexcept;

    bool linkwitzRileyActive() { return linkwitzRiley; }

private:
    //==============================================================================
    // Vector used for dynamic allocation of numerators and denominators
    // based on filter order
    std::vector<double> numerator;
    std::vector<double> denominator;
    // Vector used for dynamic allocation of delay line size based on
    // filter type
    std::vector<double> inputDelayBuf;
    std::vector<double> outputDelayBuf;
    std::vector<double>::size_type inputDelaySize, outputDelaySize;
    unsigned int inputDelayBufWritePtr, outputDelayBufWritePtr = 0;
    SpinLock processLock;
    bool linkwitzRiley;
    double prevFreq;

    JUCE_LEAK_DETECTOR (CrossoverFilter);

    // Convolution function adapted from: http://stackoverflow.com/questions/24518989/how-to-perform-1-dimensional-valid-convolution
    std::vector<double> convolveCoefficients(std::vector<double> const &f, std::vector<double> const &g);
};


#endif // __PARAMETRICEQFILTER_H_6E48F605__

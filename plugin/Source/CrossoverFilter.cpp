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

#include "CrossoverFilter.h"
#include <cmath>
#include <vector>

#if JUCE_INTEL
 #define JUCE_SNAP_TO_ZERO(n)    if (! (n < -1.0e-8f || n > 1.0e-8f)) n = 0;
#else
 #define JUCE_SNAP_TO_ZERO(n)
#endif

CrossoverFilter::CrossoverFilter(bool highpass, bool linkwitzRiley) {
    active = false;
    numerator.resize(5, 0);
    denominator.resize(5, 0);
    this->linkwitzRiley = linkwitzRiley;
    // Allocate memory for delay line based on the number of
    // coefficients generated. Initialize vectors with values of 0.
    inputDelayBuf.resize(int(numerator.size())-1, 0);
    outputDelayBuf.resize(int(denominator.size())-1, 0);
    // Store the delay size of delay buffers
    inputDelaySize = inputDelayBuf.size();
    outputDelaySize = outputDelayBuf.size();
    prevFreq = 0;
}

void CrossoverFilter::makeCrossover(
        const double crossoverFrequency,
        const int sampleRate,
        const bool linkwitzRiley,
        const bool highpass
    ) noexcept
{
    //const SpinLock::ScopedLockType sl (processLock);

    if(sampleRate < 1)
        return;
    if(crossoverFrequency == prevFreq || crossoverFrequency <= 0 || crossoverFrequency > sampleRate * 0.5)
        return;
    this->linkwitzRiley = linkwitzRiley;
    prevFreq = crossoverFrequency;

    // This code was adapted from code originally submitted by the author for
    // the Real-time DSP module assignment 1.

    // Deifine Q as the square root of 2
    static const double q = sqrt(2.0);

    // Warp the frequency to convert from continuous to discrete time cutoff
    const double wd1 = 1.0 / tan(M_PI*(crossoverFrequency/sampleRate));

    // Calculate coefficients from equation and store in a vector
    numerator[0] = 1.0 / (1.0 + q*wd1 + pow(wd1, 2));
    numerator[1] = 2 * numerator[0];
    numerator[2] = numerator[0];
    denominator[0] = 1.0;
    denominator[1] = -2.0 * (pow(wd1, 2) - 1.0) * numerator[0];
    denominator[2] = (1.0 - q * wd1 + pow(wd1, 2)) * numerator[0];

    // If the filter is a high pass filter, convert numerator
    // coefficients to reflect this
    if(highpass) {
        numerator[0] = numerator[0] * pow(wd1, 2);
        numerator[1] = -numerator[1] * pow(wd1, 2);
        numerator[2] = numerator[2] * pow(wd1, 2);
    }
    std::fill(inputDelayBuf.begin(), inputDelayBuf.end(), 0);
    std::fill(outputDelayBuf.begin(), outputDelayBuf.end(), 0);
    active = true;
    // If the filter is using the Linkwitz-Riley filter structure,
    // convolve the numerator and denominator generated for the 2nd
    // order butterworth filter with themselves. This creates the 5
    // coefficients of 2 cascaded 2nd order butterworth filters needed
    // for this filter structure.

    /*
    if(linkwitzRiley) {
        numerator = convolveCoefficients(numerator, numerator);
        denominator = convolveCoefficients(denominator, denominator);
    }
    */
}

void CrossoverFilter::applyFilter(float* const samples, float* const output, const int numSamples) noexcept {
    //const SpinLock::ScopedLockType sl (processLock);
    if(active){
        for(int i = 0; i < numSamples; ++i) {
            // Perform filtering using doubles for greater precision
            const double in = samples[i];
            // Increment the write pointer of the delay buffer storing input
            // samples
            ++inputDelayBufWritePtr;
            // Wrap values to within size of buffer. Prevents an integer
            // overflow
            inputDelayBufWritePtr = (inputDelayBufWritePtr+inputDelaySize)%inputDelaySize;

            // Increment the write pointer of the delay buffer storing output
            // samples
            ++outputDelayBufWritePtr;
            // Wrap values to within size of buffer. Prevents an integer
            // overflow
            outputDelayBufWritePtr = (outputDelayBufWritePtr+outputDelaySize)%outputDelaySize;

            // Set the current value of the input delay buffer to the value of
            // the sample provided to the function
            inputDelayBuf[(inputDelayBufWritePtr+inputDelaySize)%inputDelaySize] = in;

            // Initialize a variable to store an output value
            double y = 0;
            // Accumulate each sample in the input delay buffer, multiplied by
            // it's corresponding coefficient
            for(unsigned int j = 0; j < numerator.size(); j++) {
                y += inputDelayBuf[(inputDelayBufWritePtr-j+inputDelaySize)%inputDelaySize] * numerator[j];
            }
            // decumulate each sample in the output delay buffer (aside from
            // the current index), multiplied by it's corresponding coefficient
            for(unsigned int k = 1; k < denominator.size(); k++) {
                y -= outputDelayBuf[(outputDelayBufWritePtr-k+outputDelaySize)%outputDelaySize] * denominator[k];
            }
            // Scale by first coefficient in the denominator (always 1 in
            // current implementation, so added only for generalization of the
            // method for future use)
            y /= denominator[0];

            // Taken from Juce's IIR filter code. Deals with some bug that I
            // haven't looked in to...
            //JUCE_SNAP_TO_ZERO(y);

            // Store the calculated output sample in the output sample delay
            // buffer
            outputDelayBuf[(outputDelayBufWritePtr+outputDelaySize)%outputDelaySize] = y;

            output[i] = y;
        }
    }
}

// Could have been used for linkwitz riley coefficient calculation. Needs
// memory efficiency improvments to work in this context live...
std::vector<double> CrossoverFilter::convolveCoefficients(std::vector<double> const &f, std::vector<double> const &g) {
    // Calculate the size of input vectors
    const int nf = f.size();
    const int ng = g.size();
    // Calculate the size of output vector as the combined size of both
    // input vectors, minus 1
    const int n  = nf + ng - 1;
    // Initialize vector of the same input type as input vectors
    // Allocate memory for all elements of the output to be calculated
    std::vector<double> out(n, double());
    // For each output element...
    for(auto i(0); i < n; ++i) {
        // Calculate minimum and maximum indexes to iterate over each
        // vector
        const int jmn = (i >= ng - 1)? i - (ng - 1) : 0;
        const int jmx = (i <  nf - 1)? i : nf - 1;
        // Accumulate the multiplication of elements in both vectors,
        // based on the indexes calculated, to give the output value at
        // the current output index
        for(auto j(jmn); j <= jmx; ++j) {
            out[i] += (f[j] * g[i - j]);
        }
    }
    return out;
}

#undef JUCE_SNAP_TO_ZERO

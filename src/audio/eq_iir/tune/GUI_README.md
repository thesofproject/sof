Limitations

If you do something that puts a bunch of poles and zeros at the same location, then you should expect
bizarre results when it comes to the impulse response graph. The system does not calculate the responses
individually which results in undefined behaviour due to octave's functionality when these conditions are
met. To further clarify the scenario, create 5+ peaking filters with 0dbB gain, all with the same cutoff and
Q. Now give one of them 1dB and observe the impulse go completely haywire. Octave seems to have an issue with
handling a large number of poles and zeros at the same location. Reducing the total count of peaking filters
to below 5 should resolve the graphing issue. This does not mean you can't have more than 5 peaking filters,
it just means that if you observe this behaviour, you need to adjust your cutoffs to keep the poles and zeros
from piling up in the same location.

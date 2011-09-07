// Tweak the display transfer function to never utilize the
// top brightest colors, thus avoiding the solarization
// of white that you get on the terrible macbook tn-film
// display panels.

#include <ApplicationServices/ApplicationServices.h>

int main(int argc, char **argv)
{
	CGGammaValue min = 0, max = 14/16.0, gamma = 1;
	while (1) {
		CGSetDisplayTransferByFormula(kCGDirectMainDisplay,
			min, max, gamma,
			min, max, gamma,
			min, max, gamma);
		sleep(1);
	}
	return 0;
}

#include <iostream>

#include <AL/alc.h>

#include "wav.h"
#include "ogg.h"

int main() {
	ALCdevice *device = alcOpenDevice(NULL);

	if (!device) {
		std::cout << "Failed to open device\n";
		return 1;
	}

	ALCcontext *context = alcCreateContext(device, NULL);

	if (!context) {
		std::cout << "Failed to create context\n";
		return 1;
	}

	if (alcMakeContextCurrent(context) != ALC_TRUE) {
		std::cout << "Failed to make context current\n";
		return 1;
	}

	//wav_example();
	ogg_example();

	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}

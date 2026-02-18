#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>

#include <AL/al.h>

#include "wav.h"

static bool load_wav(const std::string &file_path, ALuint buffer);

void wav_example() {
	ALuint buffer;
	alGenBuffers(1, &buffer);

	if (!load_wav("C:/dev/openal-tutorial/sounds/door_open.wav", buffer)) {
		return;
	}

	ALuint source;
	alGenSources(1, &source);
	alSourcei(source, AL_BUFFER, buffer);
	alSourcePlay(source);

	getchar();

	alSourcei(source, AL_BUFFER, 0);
	alDeleteSources(1, &source);
	alDeleteBuffers(1, &buffer);
}

static bool load_wav(const std::string &file_path, ALuint buffer) {
	std::ifstream in_stream;
	in_stream.open(file_path, std::ios::in | std::ios::binary);

	if (!in_stream.is_open()) {
		std::cout << "Failed to open wav file\n";
		return false;
	}

	uint8_t bytes[4];
	in_stream.read(reinterpret_cast<char *>(bytes), sizeof(uint8_t) * 4);

	if (bytes[0] != 'R' || bytes[1] != 'I' || bytes[2] != 'F' || bytes[3] != 'F') {
		in_stream.close();
		return false;
	}

	in_stream.seekg(sizeof(uint32_t), std::ios::cur);		// file size

	in_stream.read(reinterpret_cast<char *>(bytes), sizeof(uint8_t) * 4);

	if (bytes[0] != 'W' || bytes[1] != 'A' || bytes[2] != 'V' || bytes[3] != 'E') {
		in_stream.close();
		return false;
	}

	in_stream.read(reinterpret_cast<char *>(bytes), sizeof(uint8_t) * 4);

	if (bytes[0] != 'f' || bytes[1] != 'm' || bytes[2] != 't' || bytes[3] != ' ') {
		in_stream.close();
		return false;
	}

	in_stream.seekg(sizeof(uint32_t), std::ios::cur);				// format data length
	in_stream.seekg(sizeof(uint16_t), std::ios::cur);				// format type
	
	uint16_t num_channels;
	in_stream.read(reinterpret_cast<char *>(&num_channels), sizeof(uint16_t));

	uint32_t sample_rate;
	in_stream.read(reinterpret_cast<char *>(&sample_rate), sizeof(uint32_t));

	in_stream.seekg(sizeof(uint32_t), std::ios::cur);				// byte rate
	in_stream.seekg(sizeof(uint16_t), std::ios::cur);				// block align

	uint16_t bits_per_sample;
	in_stream.read(reinterpret_cast<char *>(&bits_per_sample), sizeof(uint16_t));

	in_stream.read(reinterpret_cast<char *>(bytes), sizeof(uint8_t) * 4);

	if (bytes[0] != 'd' || bytes[1] != 'a' || bytes[2] != 't' || bytes[3] != 'a') {
		in_stream.close();
		return false;
	}

	ALenum format;

	if (num_channels == 1 && bits_per_sample == 8) {
		format = AL_FORMAT_MONO8;
	} else if (num_channels == 1 && bits_per_sample == 16) {
		format = AL_FORMAT_MONO16;
	} else if (num_channels == 2 && bits_per_sample == 8) {
		format = AL_FORMAT_STEREO8;
	} else if (num_channels == 2 && bits_per_sample == 16) {
		format = AL_FORMAT_STEREO16;
	} else {
		std::cout << "Unknown wav file format\n";
		in_stream.close();
		return false;
	}

	uint32_t data_size;
	in_stream.read(reinterpret_cast<char *>(&data_size), sizeof(uint32_t));

	uint8_t *data = new uint8_t[data_size];
	in_stream.read(reinterpret_cast<char *>(data), sizeof(uint8_t) * data_size);

	in_stream.close();

	alBufferData(buffer, format, data, data_size, sample_rate);

	delete[] data;

	return true;
}

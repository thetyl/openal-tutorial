#include <fstream>
#include <iostream>
#include <thread>
#include <sys/stat.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <vorbis/vorbisfile.h>

#define NUM_OGG_AL_BUFFERS 4
#define STREAM_BUFFER_SIZE 65536

struct OGG {
	ALsizei file_size;
	std::fstream file_stream;
	ALuint al_buffers[NUM_OGG_AL_BUFFERS];
	ALuint al_source;
	ALsizei bytes_consumed;
	OggVorbis_File vorbis_file;
	ALsizei sample_rate;
	ALenum format;
	int current_section;
};

static void run_ogg_thread();
static bool update_stream_buffers(bool init);
static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *datasource);
static int seek_callback(void *datasource, ogg_int64_t offset, int whence);
static long tell_callback(void *datasource);

static char *stream_buffer;
static bool ogg_thread_running;
static OGG ogg;

int main() {
	// Setup
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

	// OGG
	stream_buffer = new char[STREAM_BUFFER_SIZE];

	ogg_thread_running = true;

	std::thread ogg_thread(run_ogg_thread);

	std::getchar();

	ogg_thread_running = false;
	ogg_thread.join();

	delete[] stream_buffer;

	// Cleanup
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}

static void run_ogg_thread() {
	ogg.bytes_consumed = 0;
	ogg.current_section = 0;

	std::string file_path = "C:/dev/openal-tutorial/sounds/song.ogg";
	ogg.file_stream.open(file_path, std::ios::in | std::ios::binary);

	if (!ogg.file_stream.is_open()) {
		std::cout << "Failed to open ogg file\n";
		return;
	}

	struct stat stats;

	if (stat(file_path.c_str(), &stats) != 0) {
		std::cout << "Failed to get file size\n";
		return;
	}

	ogg.file_size = (ALsizei)stats.st_size;

	ov_callbacks callbacks;
	callbacks.read_func = read_callback;
	callbacks.close_func = NULL;
	callbacks.seek_func = seek_callback;
	callbacks.tell_func = tell_callback;

	int result = ov_open_callbacks(&ogg, &ogg.vorbis_file, NULL, -1, callbacks);

	if (result != 0) {
		std::cout << "Failed ov_open_callbacks with code " << result << "\n";
		return;
	}

	vorbis_info *info = ov_info(&ogg.vorbis_file, -1);

	if (info == NULL) {
		std::cout << "Failed ov_info\n";
		return;
	}

	ogg.sample_rate = info->rate;
	// ogg.duration = ov_time_total(&ogg.vorbis_file, -1);

	if (info->channels == 1) {
		ogg.format = AL_FORMAT_MONO16;
	} else if (info->channels == 2) {
		ogg.format = AL_FORMAT_STEREO16;
	} else {
		std::cout << "Unrecognized ogg format\n";
		return;
	}

	alGenSources(1, &ogg.al_source);
	alGenBuffers(NUM_OGG_AL_BUFFERS, &ogg.al_buffers[0]);

	ALint num_processed_buffers;
	ALuint processed_buffers[NUM_OGG_AL_BUFFERS];

	bool eof = update_stream_buffers(true);

	while (ogg_thread_running) {
		if (!eof) {
			if (update_stream_buffers(false)) {
				eof = true;
			}

			continue;
		}

		ALint state;
		alGetSourcei(ogg.al_source, AL_SOURCE_STATE, &state);

		if (state == AL_STOPPED) {
			alGetSourcei(ogg.al_source, AL_BUFFERS_PROCESSED, &num_processed_buffers);

			if (num_processed_buffers > 0) {
				alSourceUnqueueBuffers(ogg.al_source, num_processed_buffers, &processed_buffers[0]);
			}

			if (ov_clear(&ogg.vorbis_file) != 0) {
				std::cout << "Failed to clear vorbis file\n";
			}

			ogg.file_stream.close();

			break;
		}
	}
}

static bool update_stream_buffers(bool init) {
	ALint num_processed_buffers;
	ALuint processed_buffers[NUM_OGG_AL_BUFFERS];

	if (!init) {
		alGetSourcei(ogg.al_source, AL_BUFFERS_PROCESSED, &num_processed_buffers);

		if (num_processed_buffers <= 0) {
			return false;
		}

		alSourceUnqueueBuffers(ogg.al_source, num_processed_buffers, &processed_buffers[0]);
	} else {
		num_processed_buffers = NUM_OGG_AL_BUFFERS;

		for (uint32_t i = 0; i < NUM_OGG_AL_BUFFERS; ++i) {
			processed_buffers[i] = ogg.al_buffers[i];
		}
	}

	bool eof = false;

	for (uint32_t i = 0; i < num_processed_buffers && !eof; ++i) {
		uint32_t bytes_read = 0;

		while (bytes_read < STREAM_BUFFER_SIZE) {
			long ov_read_result = ov_read(&ogg.vorbis_file, &stream_buffer[bytes_read], STREAM_BUFFER_SIZE - bytes_read, 0, 2, 1, &ogg.current_section);

			if (ov_read_result == 0) {      // eof
				eof = true;
				break;
			} else if (ov_read_result < 0) {
				std::cout << "Failed ov_read with code " << ov_read_result << "\n";
			}

			bytes_read += ov_read_result;
		}

		if (bytes_read > 0) {
			alBufferData(processed_buffers[i], ogg.format, stream_buffer, bytes_read, ogg.sample_rate);
			alSourceQueueBuffers(ogg.al_source, 1, &processed_buffers[i]);

			if (i == 0 && init) {
				alSourcePlay(ogg.al_source);
			}
		}
	}

	return eof;
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *datasource) {
	OGG *ogg = (OGG *)datasource;
	ALsizei read_bytes = size * nmemb;

	if (ogg->bytes_consumed + read_bytes > ogg->file_size) {
		read_bytes = ogg->file_size - ogg->bytes_consumed;
	}

	ogg->file_stream.seekg(ogg->bytes_consumed);
	ogg->file_stream.read((char *)ptr, read_bytes);

	ogg->bytes_consumed += read_bytes;

	return read_bytes;
}

static int seek_callback(void *datasource, ogg_int64_t offset, int whence) {
	OGG *ogg = (OGG *)datasource;

	if (whence == SEEK_CUR) {
		ogg->bytes_consumed += offset;
	} else if (whence == SEEK_END) {
		ogg->bytes_consumed = ogg->file_size - offset;
	} else if (whence == SEEK_SET) {
		ogg->bytes_consumed = offset;
	} else {
		std::cout << "Invalid seek_func value\n";
		return -1;
	}

	if (ogg->bytes_consumed < 0) {
		ogg->bytes_consumed = 0;
		std::cout << "seek_func bytes_consumed can not be less than 0\n";
		return -1;
	} else if (ogg->bytes_consumed > ogg->file_size) {
		ogg->bytes_consumed = ogg->file_size;
		std::cout << "seek_func bytes_consumed can not exceed file size\n";
		return -1;
	}

	return 0;
}

static long tell_callback(void *datasource) {
	return ((OGG *)datasource)->bytes_consumed;
}

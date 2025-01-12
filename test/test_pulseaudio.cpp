// g++ -o pulseaudio-capture pulseaudio-capture.cpp -lpulse-simple -lpulse
#include <cmath>
#include <cstring>
#include <iostream>
#include <pulse/error.h>
#include <pulse/simple.h>

#define BUFSIZE 1024

void printProgressBar(float progress) {
    int barWidth = 50;
    std::cout << "[";
    int pos = static_cast<int>(round(barWidth * progress));
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) {
            std::cout << "=";
        } else {
            std::cout << " ";
        }
    }
    std::cout << "] " << int(progress * 100.0) << " %   \r";
    std::cout.flush();
}

int main(int argc, char *argv[]) {
    pa_simple *s = nullptr;
    pa_sample_spec ss;
    int error;

    ss.format = PA_SAMPLE_FLOAT32LE;
    ss.channels = 1;
    ss.rate = 44100;

    s = pa_simple_new(nullptr, "Microphone", PA_STREAM_RECORD, nullptr, "record", &ss, nullptr,
                      nullptr, &error);
    if (!s) {
        std::cerr << "pa_simple_new() failed: " << pa_strerror(error) << std::endl;
        return -1;
    }

    uint8_t buf[BUFSIZE];
    float maxAmplitude = 0.0;
    while (true) {
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            std::cerr << "pa_simple_read() failed: " << pa_strerror(error) << std::endl;
            if (s) {
                pa_simple_free(s);
            }
            return -1;
        }

        for (size_t i = 0; i < sizeof(buf); i += sizeof(float)) {
            float sample;
            std::memcpy(&sample, buf + i, sizeof(float));
            float amplitude = std::abs(sample);

            if (amplitude > maxAmplitude) {
                maxAmplitude = amplitude;
            }
        }

        printProgressBar(maxAmplitude);

        maxAmplitude = 0.0;
    }

    if (s) {
        pa_simple_free(s);
    }

    return 0;
}

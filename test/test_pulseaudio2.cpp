// g++ -o record_example test_pulseaudio2.cpp `pkg-config --cflags --libs libpulse`
#include <cstring>
#include <iostream>
#include <pulse/pulseaudio.h>

void stream_read_callback(pa_stream *s, size_t length, void *userdata) {
    const void *data;
    if (pa_stream_peek(s, &data, &length) < 0) {
        std::cerr << "Failed to peek into stream" << std::endl;
        return;
    }

    printf("Data[%p] length: %d\n", data, length);

    pa_stream_drop(s);
}

void stream_state_callback(pa_stream *s, void *userdata) {
    switch (pa_stream_get_state(s)) {
        case PA_STREAM_READY:
            std::cout << "Stream is ready." << std::endl;
            break;
        case PA_STREAM_FAILED:
            std::cerr << "Stream failed: "
                      << pa_strerror(pa_context_errno(pa_stream_get_context(s))) << std::endl;
            break;
        case PA_STREAM_TERMINATED:
            std::cerr << "Stream terminated." << std::endl;
            break;
        default:
            break;
    }
}

int main() {
    pa_mainloop *m = pa_mainloop_new();
    pa_mainloop_api *mainloop_api = pa_mainloop_get_api(m);
    pa_context *ctx = pa_context_new(mainloop_api, "Test Application");

    pa_context_connect(ctx, nullptr, PA_CONTEXT_NOFLAGS, nullptr);

    pa_context_state_t ctx_state;
    do {
        pa_mainloop_iterate(m, 1, nullptr);
        ctx_state = pa_context_get_state(ctx);
    } while (ctx_state != PA_CONTEXT_READY && ctx_state != PA_CONTEXT_FAILED);

    if (ctx_state == PA_CONTEXT_FAILED) {
        std::cerr << "Failed to connect to PulseAudio server" << std::endl;
        return -1;
    }

    pa_sample_spec spec = {};
    spec.format = PA_SAMPLE_S16LE;
    spec.rate = 44100;
    spec.channels = 2;

    pa_stream *stream = pa_stream_new(ctx, "Recording Stream", &spec, nullptr);
    pa_stream_set_read_callback(stream, stream_read_callback, nullptr);
    pa_stream_set_state_callback(stream, stream_state_callback, nullptr);

    pa_stream_connect_record(stream, nullptr, nullptr, PA_STREAM_NOFLAGS);

    while (true) {
        pa_mainloop_iterate(m, 1, nullptr);
    }

    pa_stream_disconnect(stream);
    pa_stream_unref(stream);
    pa_context_disconnect(ctx);
    pa_context_unref(ctx);
    pa_mainloop_free(m);

    return 0;
}

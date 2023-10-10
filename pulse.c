#include"share.h"
#include<pulse/error.h>
#include<pulse/pulseaudio.h>
#include<pulse/simple.h>
#include<string.h>

pa_mainloop *m_pulseaudio_mainloop;
#include <limits.h>
#include <math.h>
#include <string.h>

int write_to_input_buffers(int16_t samples, unsigned char *buf, void *data) {
    if (samples == 0)
        return 0;
    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    int bytes_per_sample = audio->format / 8;
    if (audio->samplesCounter + samples > audio->arctic_buffer_size) {
        // buffer overflow, discard what ever is in the buffer and start over
        for (uint16_t n = 0; n < audio->arctic_buffer_size; n++) {
            audio->arctic[n] = 0;
        }
        audio->samplesCounter = 0;
    }
    int n = 0;
    for (uint16_t i = 0; i < samples; i++) {
        int16_t *buf16 = (int16_t *)&buf[n];
        audio->arctic[i + audio->samplesCounter] = *buf16;
        
        n += bytes_per_sample;
    }
    audio->samplesCounter += samples;
    pthread_mutex_unlock(&audio->lock);
    return 0;
}


void cb(__attribute__((unused)) pa_context *pulseaudio_context, const pa_server_info *i,
        void *userdata) {

    // getting default sink name
    struct audio_data *audio = (struct audio_data *)userdata;
    pthread_mutex_lock(&audio->lock);
    free(audio->source);
    audio->source = malloc(sizeof(char) * 1024);

    strcpy(audio->source, i->default_sink_name);

    // appending .monitor suufix
    audio->source = strcat(audio->source, ".monitor");
    pthread_mutex_unlock(&audio->lock);

    // quiting mainloop
    pa_context_disconnect(pulseaudio_context);
    pa_context_unref(pulseaudio_context);
    pa_mainloop_quit(m_pulseaudio_mainloop, 0);
    pa_mainloop_free(m_pulseaudio_mainloop);
}

void pulseaudio_context_state_callback(pa_context *pulseaudio_context, void *userdata) {

    // make sure loop is ready
    switch (pa_context_get_state(pulseaudio_context)) {
    case PA_CONTEXT_UNCONNECTED:
        // printf("UNCONNECTED\n");
        break;
    case PA_CONTEXT_CONNECTING:
        // printf("CONNECTING\n");
        break;
    case PA_CONTEXT_AUTHORIZING:
        // printf("AUTHORIZING\n");
        break;
    case PA_CONTEXT_SETTING_NAME:
        // printf("SETTING_NAME\n");
        break;
    case PA_CONTEXT_READY: // extract default sink name
        // printf("READY\n");
        pa_operation_unref(pa_context_get_server_info(pulseaudio_context, cb, userdata));
        break;
    case PA_CONTEXT_FAILED:
        fprintf(stderr, "failed to connect to pulseaudio server\n");
        exit(EXIT_FAILURE);
        break;
    case PA_CONTEXT_TERMINATED:
        // printf("TERMINATED\n");
        pa_mainloop_quit(m_pulseaudio_mainloop, 0);
        break;
    }
}

void getPulseDefaultSink(void *data) {

    struct audio_data *audio = (struct audio_data *)data;
    pa_mainloop_api *mainloop_api;
    pa_context *pulseaudio_context;
    int ret;

    // Create a mainloop API and connection to the default server
    m_pulseaudio_mainloop = pa_mainloop_new();

    mainloop_api = pa_mainloop_get_api(m_pulseaudio_mainloop);
    pulseaudio_context = pa_context_new(mainloop_api, "cava device list");

    // This function connects to the pulse server
    pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS, NULL);

    //        printf("connecting to server\n");

    // This function defines a callback so the server will tell us its state.
    pa_context_set_state_callback(pulseaudio_context, pulseaudio_context_state_callback,
                                  (void *)audio);

    // starting a mainloop to get default sink

    // starting with one nonblokng iteration in case pulseaudio is not able to run
    if (!(ret = pa_mainloop_iterate(m_pulseaudio_mainloop, 0, &ret))) {
        fprintf(stderr,
                "Could not open pulseaudio mainloop to "
                "find default device name: %d\n"
                "check if pulseaudio is running\n",
                ret);

        exit(EXIT_FAILURE);
    }

    pa_mainloop_run(m_pulseaudio_mainloop, &ret);
}

void *input_pulse(void *data) {

    struct audio_data *audio = (struct audio_data *)data;
    uint16_t frames = audio->arctic_buffer_size / audio->channels;
    unsigned char buf[audio->arctic_buffer_size* audio->format / 8];
    
	/* The sample type to use */
    static const pa_sample_spec ss = {.format = PA_SAMPLE_S16LE, .rate = 44100, .channels = 2};

    const int frag_size = frames * audio->channels * audio->format / 8 *
                          2; // we double this because of cpu performance issues with pulseaudio

    pa_buffer_attr pb = {.maxlength = (uint32_t)-1, // BUFSIZE * 2,
                         .fragsize = frag_size};

    pa_simple *s = NULL;
    int error;

    if (!(s = pa_simple_new(NULL, "cava", PA_STREAM_RECORD, audio->source, "audio for cava", &ss,
                            NULL, &pb, &error))) {
        audio->terminate = 1;
        pthread_exit(NULL);
        return 0;
    }

    while (!audio->terminate) {
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {

            audio->terminate = 1;
        }

        write_to_input_buffers(audio->arctic_buffer_size, buf, data);
    }

    pa_simple_free(s);
    pthread_exit(NULL);
    return 0;
}


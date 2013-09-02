/*
 * Copyright © 2013 Sebastien Alaiwan <sebastien.alaiwan@gmail.com>
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb api/function exhaustive test. Plays a series of tones in different
 * conditions. */
#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include "cubeb/cubeb.h"
#include "common.h"

#define MAX_NUM_CHANNELS 32

#if !defined(M_PI)
#define M_PI 3.14159265358979323846
#endif

#define NELEMS(x) ((int) (sizeof(x) / sizeof(x[0])))
#define VOLUME 0.2

float get_frequency(int channel_index)
{
  return 220.0f * (channel_index+1);
}

/* store the phase of the generated waveform */
typedef struct {
  int num_channels;
  float phase[MAX_NUM_CHANNELS];
  float sample_rate;
} synth_state;

synth_state* synth_create(int num_channels, float sample_rate)
{
  synth_state* synth = malloc(sizeof(synth_state));
  for(int i=0;i < MAX_NUM_CHANNELS;++i)
    synth->phase[i] = 0.0f;
  synth->num_channels = num_channels;
  synth->sample_rate = sample_rate;
  return synth;
}

void synth_destroy(synth_state* synth)
{
  free(synth);
}

void synth_run_float(synth_state* synth, float* audiobuffer, long nframes)
{
  for(int c=0;c < synth->num_channels;++c) {
    float freq = get_frequency(c);
    float phase_inc = 2.0 * M_PI * freq / synth->sample_rate;
    for(long n=0;n < nframes;++n) {
      audiobuffer[n*synth->num_channels+c] = sin(synth->phase[c]) * VOLUME;
      synth->phase[c] += phase_inc;
    }
  }
}

long data_cb_float(cubeb_stream *stream, void *user, void *buffer, long nframes)
{
  synth_state *synth = (synth_state *)user;
  synth_run_float(synth, (float*)buffer, nframes);
  return nframes;
}

void synth_run_16bit(synth_state* synth, short* audiobuffer, long nframes)
{
  for(int c=0;c < synth->num_channels;++c) {
    float freq = get_frequency(c);
    float phase_inc = 2.0 * M_PI * freq / synth->sample_rate;
    for(long n=0;n < nframes;++n) {
      audiobuffer[n*synth->num_channels+c] = sin(synth->phase[c]) * VOLUME * 32767.0f;
      synth->phase[c] += phase_inc;
    }
  }
}

long data_cb_short(cubeb_stream *stream, void *user, void *buffer, long nframes)
{
  synth_state *synth = (synth_state *)user;
  synth_run_16bit(synth, (short*)buffer, nframes);
  return nframes;
}

void state_cb(cubeb_stream *stream, void *user, cubeb_state state)
{
}

int run_test(int num_channels, int sampling_rate, int is_float)
{
  int ret = CUBEB_OK;

  cubeb *ctx = NULL;
  synth_state* synth = NULL;
  cubeb_stream *stream = NULL;

  ret = cubeb_init(&ctx, "Cubeb audio test");
  if (ret != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    goto cleanup;
  }

  fprintf(stderr, "Testing %d channel(s), %d Hz, %s (%s)\n", num_channels, sampling_rate, is_float ? "float" : "short", cubeb_get_backend_id(ctx));

  cubeb_stream_params params;
  params.format = is_float ? CUBEB_SAMPLE_FLOAT32NE : CUBEB_SAMPLE_S16NE;
  params.rate = sampling_rate;
  params.channels = num_channels;

  synth = synth_create(params.channels, params.rate);
  if (synth == NULL) {
    fprintf(stderr, "Out of memory\n");
    goto cleanup;
  }

  ret = cubeb_stream_init(ctx, &stream, "test tone", params,
                          250, is_float ? data_cb_float : data_cb_short, state_cb, synth);
  if (ret != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream: %d\n", ret);
    goto cleanup;
  }

  cubeb_stream_start(stream);
  delay(200);
  cubeb_stream_stop(stream);

cleanup:
  cubeb_stream_destroy(stream);
  cubeb_destroy(ctx);
  synth_destroy(synth);

  return ret;
}

int main(int argc, char *argv[])
{
  int ret;

  int channel_values[] = {
    1,
    2,
    4,
    5,
    6,
  };

  int freq_values[] = {
    24000,
    44100,
    48000,
  };

  for(int j=0;j < NELEMS(channel_values);++j) {
    for(int i=0;i < NELEMS(freq_values);++i) {
      assert(channel_values[j] < MAX_NUM_CHANNELS);
      fprintf(stderr, "--------------------------\n");
      run_test(channel_values[j], freq_values[i], 0);
      run_test(channel_values[j], freq_values[i], 1);
    }
  }

  return CUBEB_OK;
}


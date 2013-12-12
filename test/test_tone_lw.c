/*
 * Copyright © 2011 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

/* libcubeb lw api/function test. Plays a simple tone. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "cubeb/cubeb_lw.h"

#define SAMPLE_FREQUENCY 44100
#define CHANNELS 2

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* store the phase of the generated waveform */
struct cb_user_data {
  long position;
};

long data_cb(cubeb_stream *stream, void *user, void *buffer, long nframes)
{
  struct cb_user_data *u = (struct cb_user_data *)user;
  float *b = buffer;
  int i;

  if (stream == NULL || u == NULL)
    return CUBEB_ERROR;

  /* generate our test tone on the fly */
  for (i = 0; i < nframes * CHANNELS; i += CHANNELS) {
    /* North American dial tone */
    b[i]     = 0.66*sin(2*M_PI*((i/2) + u->position)*350/SAMPLE_FREQUENCY);
    b[i + 1] = 0.66*sin(2*M_PI*((i/2) + u->position)*440/SAMPLE_FREQUENCY);
    /* European dial tone */
    /*b[i]  = 30000*sin(2*M_PI*(i + u->position)*425/SAMPLE_FREQUENCY);*/
  }
  /* remember our phase to avoid clicking on buffer transitions */
  /* we'll still click if position overflows */
  u->position += nframes;

  return nframes;
}

void state_cb(cubeb_stream *stream, void *user, cubeb_state state)
{
  struct cb_user_data *u = (struct cb_user_data *)user;

  if (stream == NULL || u == NULL)
    return;

  switch (state) {
    case CUBEB_STATE_STARTED:
      printf("stream started\n"); break;
    case CUBEB_STATE_STOPPED:
      printf("stream stopped\n"); break;
    case CUBEB_STATE_DRAINED:
      printf("stream drained\n"); break;
    default:
      printf("unknown stream state %d\n", state);
  }

  return;
}

int main(int argc, char *argv[])
{
  cubeb_lw *ctx;
  cubeb_lw_stream *stream;
  cubeb_stream_params params;
  struct cb_user_data *user_data;
  int ret;

  ret = cubeb_lw_init(&ctx, "Cubeb tone example");
  if (ret != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb library\n");
    return ret;
  }

  params.format = CUBEB_SAMPLE_FLOAT32NE;
  params.rate = SAMPLE_FREQUENCY;
  params.channels = CHANNELS;

  user_data = malloc(sizeof(*user_data));
  if (user_data == NULL) {
    fprintf(stderr, "Error allocating user data\n");
    return CUBEB_ERROR;
  }
  user_data->position = 0;

  ret = cubeb_lw_stream_init(ctx, &stream, "Cubeb tone", params,
                          100, data_cb, state_cb, user_data);
  if (ret != CUBEB_OK) {
    fprintf(stderr, "Error initializing cubeb stream\n");
    return ret;
  }

  cubeb_lw_stream_start(stream);
  sleep(1);
  cubeb_lw_stream_stop(stream);

  cubeb_lw_stream_destroy(stream);
  cubeb_lw_destroy(ctx);

  free(user_data);

  return CUBEB_OK;
}

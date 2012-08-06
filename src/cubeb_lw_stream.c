/*
 * Copyright © 2012 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "cubeb/cubeb.h"
#include "cubeb/cubeb_lw.h"

/* TODO
   - support lwstream volumes
     - support stream volumes, too
     - PA flat volume style?
   - support all sample formats
   - support all channels (provide mixup/down)
   - support all sample rates (provide resampler)
   - support multiple latencies (low priority)
   - support alternate buffer formats (non-interleaved)
 */

#define CUBEB_OUTPUT_RATE 44100
#define CUBEB_OUTPUT_CHANNELS 2
#define CUBEB_LATENCY 100
#define CUBEB_STREAM_MAX 16

struct cubeb_lw_stream {
  int inuse;
  int running;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * user_ptr;
};

struct cubeb_lw {
  cubeb * context;
  cubeb_stream * stream;
  cubeb_lw_stream streams[16];
  size_t bytes_per_frame;
  cubeb_stream_params params;
};

static float
clamp(float f)
{
  if (f > 1.0)
    return 1.0;
  if (f < -1.0)
    return -1.0;
  return f;
}

static void
mix_into(void * destination, void * source, long nframes, cubeb_sample_format format)
{
  assert(format == CUBEB_SAMPLE_FLOAT32NE);
  float * dst = destination;
  float * src = source;

  for (int i = 0; i < nframes * 2; i += 2) {
    dst[i] = clamp(dst[i] + src[i]);
    dst[i + 1] = clamp(dst[i + 1] + src[i + 1]);
  }
}

static long
cubeb_lw_data_callback(cubeb_stream * stream, void * user_ptr, void * buffer, long nframes)
{
  cubeb_lw * ctx = user_ptr;
  void * mixbuffer;
  cubeb_lw_stream * lwstream;
  long got;

  // XXX assumes zeroed memory is equivalent to IEEE 0.0 float
  memset(buffer, 0, nframes * ctx->bytes_per_frame);

  // XXX assumes zeroed memory is equivalent to IEEE 0.0 float
  // XXX this doesn't need to be zeroed, also rename it to something temp-sounding
  mixbuffer = calloc(nframes, ctx->bytes_per_frame);
  assert(mixbuffer);

  // XXX assume all lwstreams use same sample format
  for (int i = 0; i < CUBEB_STREAM_MAX; ++i) {
    lwstream = &ctx->streams[i];
    if (!lwstream->inuse || !lwstream->running) {
      continue;
    }

    got = lwstream->data_callback((cubeb_stream *) lwstream, lwstream->user_ptr, mixbuffer, nframes);
    assert(got == nframes); // XXX handle drain later

    mix_into(buffer, mixbuffer, nframes, ctx->params.format); // XXX need src and dst formats
  }

  free(mixbuffer);
  return nframes; // XXX handle drain
}

static void
cubeb_lw_state_callback(cubeb_stream * stream, void * user_ptr, cubeb_state state)
{
}

int
cubeb_lw_init(cubeb_lw ** context, char const * context_name)
{
  cubeb_lw * ctx;
  int r;

  assert(context);
  ctx = calloc(1, sizeof(*ctx));

  r = cubeb_init(&ctx->context, "Cubeb mixer");
  if (r != CUBEB_OK) {
    cubeb_lw_destroy(ctx);
    return r; // XXX reflect internal library errors or remap?
  }

  // XXX cubeb needs a way to query the hardware's preferred settings
  ctx->params.format = CUBEB_SAMPLE_FLOAT32NE;
  ctx->params.rate = CUBEB_OUTPUT_RATE;
  ctx->params.channels = CUBEB_OUTPUT_CHANNELS;

  ctx->bytes_per_frame = sizeof(float) * ctx->params.channels;

  r = cubeb_stream_init(ctx->context, &ctx->stream, "Cubeb mixer - output",
                        ctx->params, CUBEB_LATENCY, cubeb_lw_data_callback,
                        cubeb_lw_state_callback, ctx);
  if (r != CUBEB_OK) {
    cubeb_lw_destroy(ctx);
    return r;
  }

  // XXX the stream runs all the time, fix this so it only runs when there
  // are active lwstreams to save resources
  r = cubeb_stream_start(ctx->stream);
  if (r != CUBEB_OK) {
    cubeb_lw_destroy(ctx);
    return r;
  }

  *context = ctx;
  return CUBEB_OK;
}

void
cubeb_lw_destroy(cubeb_lw * context)
{
  if (context->stream) {
    cubeb_stream_stop(context->stream);
    cubeb_stream_destroy(context->stream);
  }
  if (context->context) {
    cubeb_destroy(context->context);
  }
  free(context);
}

int
cubeb_lw_stream_init(cubeb_lw * context, cubeb_lw_stream ** stream, char const * stream_name,
                     cubeb_stream_params stream_params, unsigned int latency,
                     cubeb_data_callback data_callback,
                     cubeb_state_callback state_callback,
                     void * user_ptr)
{
  cubeb_lw_stream * stm;
  // XXX ignore stream_params and latency for now
  assert(context);
  assert(stream);

  if (stream_params.format != context->params.format ||
      stream_params.rate != context->params.rate ||
      stream_params.channels != context->params.channels) {
    return CUBEB_ERROR;
  }

  stm = NULL;
  for (int i = 0; i < CUBEB_STREAM_MAX; ++i) {
    if (!context->streams[i].inuse) {
      stm = &context->streams[i];
      stm->inuse = 1;
      break;
    }
  }
  if (stm == NULL) {
    return CUBEB_ERROR;
  }

  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;

  *stream = stm;
  return CUBEB_OK;
}

void
cubeb_lw_stream_destroy(cubeb_lw_stream * stream)
{
  assert(stream);
  memset(stream, 0, sizeof(*stream));
  stream->inuse = 0;
}

int
cubeb_lw_stream_start(cubeb_lw_stream * stream)
{
  assert(stream);
  stream->running = 1;
  return CUBEB_OK;
}

int
cubeb_lw_stream_stop(cubeb_lw_stream * stream)
{
  assert(stream);
  stream->running = 0;
  return CUBEB_OK;
}

int
cubeb_lw_stream_get_position(cubeb_lw_stream * stream, uint64_t * position)
{
  assert(stream);
  *position = 0;
  return CUBEB_OK;
}

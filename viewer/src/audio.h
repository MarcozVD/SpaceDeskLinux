/* Speaker session: a second connection (client type 3) that streams PCM audio. */
#ifndef SDV_AUDIO_H
#define SDV_AUDIO_H

struct sdv_audio;

struct sdv_audio *sdv_audio_start(const char *host, int port, const char *device_id, const char *name);
void sdv_audio_stop(struct sdv_audio *a);
/* 1 while audio is playing, 0 while connecting, -1 after an error (message in err). */
int sdv_audio_status(struct sdv_audio *a, char *err, int errlen);

#endif

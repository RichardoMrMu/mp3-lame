#include "lame/lame.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
//#include <signal.h>
//#include <sys/ioctl.h>
#include <memory.h>
//#include <linux/soundcard.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include<signal.h>
#define TIMES 10    //录音时间,秒
#define RATE 44100 //采样频率
#define BITS 16    //量化位数
#define CHANNELS 2    //声道数目
#define INBUFF_SIZE 4096
#define MP3BUFF_SIZE (int) (1.25 * INBUFF_SIZE) + 7200

// handle the case of underrun or overrun
int xrun(snd_pcm_t *handle);

// handle the case that device busy
int suspend(snd_pcm_t *handle);

// set alsa params
int SetFormat(snd_pcm_t *handle, unsigned int channels, unsigned int rate) {
    int ret = 0;

    ret = snd_pcm_set_params(handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 500000);
    if (ret != 0) {
        printf("Unable to set params,error(%s)\n", snd_strerror(ret));
    }

    return ret;
}

int main(int argc, char **argv) {
    int fd_dsp;
    FILE *fd_tmp;
    FILE *fd_mp3;
    lame_global_flags *gfp;
    short *input_buff;
    unsigned char *mp3_buff;

    int samples;
    int mp3_bytes;
    int write_bytes;
    int num = 0;
    int ch;
    int i = 0;
    int ret = 0;
    snd_pcm_t *handle;

    if (argc != 2) {
        fprintf(stderr, "Useage: ./mp3_record test.mp3\n");
        return -1;
    }

    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0) != 0) {
        printf("Failed to open device\n");
        return -1;
    }
    if (SetFormat(handle, CHANNELS, RATE) != 0) {
        printf("Cannot set sound device in bit 16, channel 2, speed44100.\n");
        return -1;
    }

    if ((fd_mp3 = fopen(argv[1], "w")) == NULL) {
        fprintf(stderr, "Open file error: %s\n", strerror(errno));
        ret = -1;
        goto CLOSE_DSP;
    }
    gfp = lame_init();
    if (gfp == NULL) {
        printf("lame_init failed\n");
        ret = -1;
        goto CLOSE_MP3;
    }

    lame_set_in_samplerate(gfp, RATE);
    //lame_set_out_samplerate(gfp, RATE);
    lame_set_num_channels(gfp, CHANNELS);
    //lame_set_brate(gfp, 24);
    //lame_set_VBR_min_bitrate_kbps(gfp, lame_get_brate(gfp));
    //lame_set_quality(gfp,7);
    ret = lame_init_params(gfp);
    if (ret < 0) {
        printf("lame_init_params returned %d\n", ret);
        ret = -1;
        goto CLOSE_LAME;
    }
    ch = lame_get_num_channels(gfp);
    printf("default rate is %d, out rate is %d.\n", lame_get_in_samplerate(gfp), lame_get_out_samplerate(gfp));
    printf("default brate is %d.\n", lame_get_brate(gfp));
    printf("default samples is %lu.\n", lame_get_num_samples(gfp));
    printf("default channel is %d.\n", ch);
    printf("default vbr_mode is %d.\n", lame_get_VBR(gfp));
    printf("default force_ms is %d.\n", lame_get_force_ms(gfp));
    printf("default mode is %d.\n", lame_get_mode(gfp));
    printf("default compression_ratio is %f.\n", lame_get_compression_ratio(gfp));
    printf("default VBR_mean_bitrate is %d.\n", lame_get_VBR_mean_bitrate_kbps(gfp));
    printf("default quality is %d.\n", lame_get_quality(gfp));


    input_buff = (short *) malloc(INBUFF_SIZE * CHANNELS);
    mp3_buff = (unsigned char *) malloc(MP3BUFF_SIZE);
    if (input_buff == NULL || mp3_buff == NULL) {
        printf("Failed to malloc.\n");
        goto FREE_BUF;
    }

    if ((TIMES*RATE * BITS * CHANNELS / 8) % (INBUFF_SIZE * CHANNELS) != 0)
        num = (TIMES*RATE * BITS * CHANNELS / 8) / (INBUFF_SIZE * CHANNELS) + 1;
    else
        num = (TIMES*RATE * BITS * CHANNELS / 8) / (INBUFF_SIZE * CHANNELS);
    for (i = 0; i < num; i++) {
        memset(input_buff, 0, INBUFF_SIZE * CHANNELS);
        memset(mp3_buff, 0, MP3BUFF_SIZE);
#if 0
        samples = read(fd_dsp, input_buff, INBUFF_SIZE * 2);
              if (samples < 0) {
                      perror("Read sound device failed");
                      ret = -1;
                      goto FREE_BUF;
              }
                      printf("samples is %d.\n", samples);
#endif


        samples = ret = snd_pcm_readi(handle, input_buff, INBUFF_SIZE / 2);
        if (ret == -EAGAIN || (ret >= 0 && (size_t) ret < INBUFF_SIZE / 2)) {
            snd_pcm_wait(handle, 1000);
            goto FREE_BUF;
        } else if (ret == -EBADFD) {      // PCM isnot in the right state
            printf("PCMis not in the right state!\n "); // EPIPE means underrun
            goto FREE_BUF;
        } else if (ret == -EPIPE) {              // an overrun occurred
            printf("anoverrun occurred. samples = %d\n", samples);
            if (xrun(handle) < 0) {
                goto FREE_BUF;
            }

        }
        else if(ret == -ESTRPIPE)
        {    // asuspend event occurred
            printf("asuspend event occurred\n");
            if (suspend(handle) < 0) {
                goto FREE_BUF;
            }
        }

        //printf("samples is %d.\n", samples);


        mp3_bytes = lame_encode_buffer_interleaved(gfp, input_buff, samples, mp3_buff, MP3BUFF_SIZE);
        //mp3_bytes= lame_encode_buffer(gfp, input_buff, NULL, samples, mp3_buff,MP3BUFF_SIZE);
        //printf("mp3_bytes is %d.\n", mp3_bytes);
        if (mp3_bytes < 0) {
            printf("lame_encode_buffer_interleaved returned %d\n", mp3_bytes);
            ret = -1;
            goto FREE_BUF;
        }
        write_bytes = fwrite(mp3_buff, 1, mp3_bytes, fd_mp3);
        if (write_bytes < 0) {
            perror("Write sound data file failed\n");
            ret = -1;
            goto FREE_BUF;
        }
        else
	    fflush(fd_mp3);
    }
    mp3_bytes = lame_encode_flush(gfp, mp3_buff, sizeof(mp3_buff));
    if (mp3_bytes > 0) {
        printf("Writing %d mp3 bytes.\n", mp3_bytes);
        if (fwrite(mp3_buff, 1, mp3_bytes, fd_mp3) != (unsigned int) mp3_bytes)
            printf("'Writing mp3 bytes error.\n");
        else
            fflush(fd_mp3);
    } else
        printf("Writing mp3 bytes 0.\n");

    FREE_BUF:
    if (mp3_buff != NULL) {
        free(mp3_buff);
        mp3_buff = NULL;
    }
    if (input_buff != NULL) {
        free(input_buff);
        input_buff = NULL;
    }

    CLOSE_LAME:
    lame_close(gfp);
    CLOSE_MP3:
    fclose(fd_mp3);
    CLOSE_DSP:
    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    EXIT:
    return ret;
}

int xrun(snd_pcm_t *handle) {
    snd_pcm_status_t *status = NULL;
    int ret;
    int verbose = 0;

    snd_pcm_status_alloca(&status);
    if ((ret = snd_pcm_status(handle, status)) < 0) {
        printf("status error: %d \n", ret);
        return -1;
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
        struct timeval now, diff, tstamp;
        gettimeofday(&now, 0);
        snd_pcm_status_get_trigger_tstamp(status, &tstamp);
        timersub(&now, &tstamp, &diff);
        fprintf(stderr, "overrun!!! (at least %.3f ms long)\n", diff.tv_sec * 1000 + diff.tv_usec / 1000.0);
        if (verbose) {
            printf("Status:\n");
            //snd_pcm_status_dump(status, _log);
        }

        if ((ret = snd_pcm_prepare(handle)) < 0) {
            printf("xrun: prepare error:%d\n", ret);
            return -1;
        }
        return 1;        // ok, data should be accepted again
    }

    if (snd_pcm_status_get_state(status) == SND_PCM_STATE_DRAINING) {
        if (verbose) {
            printf("Status(DRAINING):\n");
            //snd_pcm_status_dump(status, _log);
        }

        printf("capture stream format change? attemptingrecover...\n");
        if ((ret = snd_pcm_prepare(handle)) < 0) {
            printf("xrun(DRAINING): prepare error: %d\n", ret);
            return -1;
        }
        return 1;
    }
    if (verbose) {
        printf("Status(R/W):\n");
        //snd_pcm_status_dump(status, _log);
    }
    printf("read/write error, state = %s\n", snd_pcm_state_name(snd_pcm_status_get_state(status)));
    return -1;
}

// handle the case that device busy
int suspend(snd_pcm_t *handle) {
    int ret;

    while ((ret = snd_pcm_resume(handle)) == -EAGAIN)      //Resume from suspend.
        sleep(1);        // resume can't be proceed immediately, waituntil suspend flag is released

    if (ret < 0) {    //-ENOSYS hardware doesn't support this feature
        if ((ret = snd_pcm_prepare(handle)) < 0) {
            printf("suspend: prepare error: %d", ret);
            return -1;
        }
    }
    return 1;
}

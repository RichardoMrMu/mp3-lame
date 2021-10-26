/*=============================================================================  
#     FileName: pcm_encoder_mp3.c  
#         Desc: use lame encode pcm data to mp3 format, the pcm data 
#				read from alsa  
#       Author: licaibiao  
#   LastChange: 2017-03-27   
=============================================================================*/ 
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <lame/lame.h>
#define INBUFSIZE 128
#define MP3BUFSIZE (int) (1.25 * INBUFSIZE) + 7200
 
lame_global_flags	*gfp;
short				*input_buffer;
unsigned char 				*mp3_buffer;
char 				*outPath = "out.mp3";
FILE				*infp;
FILE				*outfp;
 
snd_pcm_hw_params_t *params;
snd_pcm_uframes_t   frames;
snd_pcm_t 			*handle;
int 				size;
short 				*alsa_buffer;
 
void lame_init_set(void)
{
	int ret_code;
    gfp = lame_init();
    if (gfp == NULL) 
    {
          printf("lame_init failed/n");
    }
 
    ret_code = lame_init_params(gfp);
    if (ret_code < 0)
    {
         printf("lame_init_params returned %d/n",ret_code);
    }
    outfp = fopen(outPath, "wb");	
}
 
void lame_alloc_buffer(void)
{
	input_buffer = (short*)malloc(INBUFSIZE*2);
    mp3_buffer = (unsigned char*)malloc(MP3BUFSIZE);	
}
 
void lame_release(void)
{
    free(mp3_buffer);
    free(input_buffer);
    fclose(outfp);
    lame_close(gfp);
}
 
void alsa_init(void){
	unsigned int 		val;
	int dir;
	int ret;
 
	ret = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (ret < 0) {
    	fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(ret));
   		 exit(1);
	}
 
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(handle, params);
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(handle, params, 2);
	val = 44100;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	frames = 32;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	ret = snd_pcm_hw_params(handle, params);
	if (ret < 0) {
    	fprintf(stderr,"unable to set hw parameters: %s\n", snd_strerror(ret));
    	exit(1);
	}
}
 
void alsa_alloc_buffer(void){
	int dir;
 
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	size = frames * 4; 
	alsa_buffer = (short *) malloc(size);
	
}
 
void alsa_release(void){
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(alsa_buffer);
}
void lame_encode(void) 
{
	int input_samples;
	int mp3_bytes;
	int status = 0;
	int ret = 0;
	int loop = 10000;
 
	while(loop--){
		ret = snd_pcm_readi(handle, alsa_buffer, frames);
		if (ret == -EPIPE){
			fprintf(stderr, "overrun occurred\n");
			snd_pcm_prepare(handle);
		}else if(ret == -EBADFD){
			printf("PCM is not in the right state \n");
		}
		else if(ret == -ESTRPIPE){
			printf("stream is suspended and waiting for an application recovery \n");
		}
		 else if (ret < 0){
			fprintf(stderr, "error from read: %s\n",snd_strerror(ret));
		}
		 else if (ret != (int)frames){
			 fprintf(stderr, "short read, read %d frames\n", ret);
		}
		else if (ret == 0){
			printf(" pcm read 0 frame deta \n ");
		}
 
	    memcpy(input_buffer, alsa_buffer, size);
		mp3_bytes = lame_encode_buffer_interleaved(gfp, input_buffer, size/4, mp3_buffer, MP3BUFSIZE);
		if (mp3_bytes < 0) 
		{
			  printf("lame_encode_buffer_interleaved returned %d \n", mp3_bytes);
		} 
		else if(mp3_bytes > 0) 
		{
			 fwrite(mp3_buffer, 1, mp3_bytes, outfp);
		}
	
	}
 
	mp3_bytes = lame_encode_flush(gfp, mp3_buffer, sizeof(mp3_buffer));
	if (mp3_bytes > 0) 
	{
		  printf("writing %d mp3 bytes\n", mp3_bytes);
		  fwrite(mp3_buffer, 1, mp3_bytes, outfp);
	}
}
 
int main(int argc, char** argv)
{
	lame_init_set();
	lame_alloc_buffer();
	alsa_init();
	alsa_alloc_buffer();
	lame_encode();
	lame_release();
	alsa_release();
}



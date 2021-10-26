
/*
read from the default PCM device and writes to standard output for 5 seconds of data
修改声音采集配置时候，除了修改声音通道数量，还应该考虑申请的缓冲区时候足够大 
*/
 
#define ALSA_PCM_NEW_HW_PARAMS_API
 
#include <alsa/asoundlib.h>
 
int main()
{
   long loops;		//一个长整型变量， 
   int rc;			//一个int变量 ,用来存放 snd_pcm_open（访问硬件）的返回值 
   int size;		//一个int变量 
   snd_pcm_t * handle;		// 一个指向snd_pcm_t的指针 
   snd_pcm_hw_params_t * params;	// 一个指向 snd_pcm_hw_params_t的指针 
   unsigned int val;		// 无符号整型变量 ，用来存放录音时候的采样率 
   int dir;			// 整型变量 
   snd_pcm_uframes_t frames;		// snd_pcm_uframes_t 型变量 
   char * buffer;		// 一个字符型指针 
   FILE * out_fd;		// 一个指向文件的指针 
   out_fd = fopen("out_pcm.raw","wb+");		/* 将流与文件之间的关系建立起来，文
											   件名为 out_pcm.raw，w是以文本方式
											   打开文件，wb是二进制方式打开文件wb+ 
											   读写打开或建立一个二进制文件，允许读和写。*/ 
   /* open PCM device for recording (capture). */
   // 访问硬件，并判断硬件是否访问成功 
   rc = snd_pcm_open(&handle, "default",SND_PCM_STREAM_CAPTURE,0);
   if( rc < 0 )
   {
      fprintf(stderr,
              "unable to open pcm device: %s\n",
              snd_strerror(rc));
      exit(1);
   }
   /* allocate a hardware parameters object */
   // 分配一个硬件变量对象 
   snd_pcm_hw_params_alloca(&params);
   /* fill it with default values. */
   // 按照默认设置对硬件对象进行设置 
   snd_pcm_hw_params_any(handle,params);
   /* set the desired hardware parameters */
   /* interleaved mode 设置数据为交叉模式*/
   snd_pcm_hw_params_set_access(handle,params,
                                SND_PCM_ACCESS_RW_INTERLEAVED);
   /* signed 16-bit little-endian format */
   // 设置数据编码格式为PCM、有符号、16bit、LE格式 
   snd_pcm_hw_params_set_format(handle,params,
                                SND_PCM_FORMAT_S16_LE);
   /* two channels(stereo) */
   // 设置双声道立体声 
   snd_pcm_hw_params_set_channels(handle,params,6);
   /* sampling rate */
   // 设置采样率 
   val = 44100;
   snd_pcm_hw_params_set_rate_near(handle,params,&val,&dir);
   /* set period size */
   // 周期长度（帧数） 
   frames = 32;
   snd_pcm_hw_params_set_period_size_near(handle,params,&frames,&dir);
   /* write parameters to the driver */
   // 将配置写入驱动程序中
   // 判断是否已经配置正确 
   rc = snd_pcm_hw_params(handle,params);
   if ( rc < 0 )
   {
       fprintf(stderr,
               "unable to set hw parameters: %s\n",
               snd_strerror(rc));
       exit(1);
   }
   /* use a buffer large enough to hold one period */
   // 配置一个缓冲区用来缓冲数据，缓冲区要足够大，此处看意思应该是只配置了
   // 够两个声道用的缓冲内存 
   snd_pcm_hw_params_get_period_size(params,&frames,&dir);
   size = frames * 12; /* 2 bytes/sample, 2channels */
   buffer = ( char * ) malloc(size);
   /* loop for 5 seconds */
   // 记录五秒长的声音 
   snd_pcm_hw_params_get_period_time(params, &val, &dir);
   loops = 5000000 / val;
   while( loops > 0 )
   {
       loops--;
       rc = snd_pcm_readi(handle,buffer,frames);
       if ( rc == -EPIPE )
       {
          /* EPIPE means overrun */
          fprintf(stderr,"overrun occured\n");
          snd_pcm_prepare(handle);
       }
       else if ( rc < 0 )
       {
          fprintf(stderr,"error from read: %s\n",
                  snd_strerror(rc));
       }
       else if ( rc != (int)frames)
       {
          fprintf(stderr,"short read, read %d frames\n",rc);
       }
       // 将音频数据写入文件 
       rc = fwrite(buffer, 1, size, out_fd);
       // rc = write(1, buffer, size);
       if ( rc != size )
       {
          fprintf(stderr,"short write: wrote %d bytes\n",rc);
       }
   }
   snd_pcm_drain(handle);
   snd_pcm_close(handle);
   free(buffer);
   fclose(out_fd);


}

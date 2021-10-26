# mp3-lame
mp3 record in jetson or linux 
## Use
you can use lame_new.cpp to test your audio recorder is working well.

```shell
g++ lame_new.cpp -o lame_new -lasound -lmp3lame
./lame_new
```

if `out.mp3` is in your dir, and open it and listen it, which has audio data, it means that your audio recorder works well.

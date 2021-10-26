from pydub import AudioSegment 
raw_audio = AudioSegment.from_file("out_pcm.raw",format='raw',frame_rate=44100,channels=2,sample_width=2)
raw_audio.export("audio1.mp3",format='mp3')

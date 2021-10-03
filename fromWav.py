from scipy.io import wavfile
samplerate, data = wavfile.read('e3.wav')

data = data[:2048]

with open('a.txt','w+') as f:
	for d in data:
		to_write = str(d)
		f.write(to_write)
		f.write(',')
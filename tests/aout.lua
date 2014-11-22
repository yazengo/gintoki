
r = pexec('avconv -i testaudios/10s-1.mp3 -f s16le -ar 44100 -ac 2 -', 'r')
w = aout()
pcopy(r, w)


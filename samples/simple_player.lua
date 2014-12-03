
require('cmd')
require('audio')
require('pipe')

audio.pipe(audio.decoder('/home/xb/SugrROM/musics/He Said She Said.mp3'), audio.out())


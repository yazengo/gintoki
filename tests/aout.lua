
require('pipe')
require('audio')

pipe.copy(audio.noise(), audio.out(), 'brw')


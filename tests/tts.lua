
require('tts')
require('cmd')

play = function (path)
	audio.play{url=path}
end

input.cmds = {
	[[ tts.download('happy father day', play) ]],
	[[ tts.download('hello world', play) ]],
	[[ tts.download('睡了吗', play) ]],
	[[ tts.download('呵呵', play) ]],
	[[ tts.download('草泥马', play) ]],
}


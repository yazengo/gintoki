
local P

local urls = {
	'http://img.xiami.net/images/album/img48/54548/299932.jpg',
	'http://sfault-avatar.b0.upaiyun.com/143/624/1436242287-1030000000158255_huge128',
	'http://img.xiami.net/images/album/img94/10594/572641372578067.jpg',
}

R.songs = {
	{title='扛把子的光辉照大地', artist='习近平', album='Best of K.B.Z', duration=10, cover_url=urls[1], id='01'},
	{title='Superstar K.B.Z', artist='S.H.E', album='Essential of K.B.Z', duration=10, cover_url=urls[2], id='02'},
	{title='唱支山歌给 K.B.Z 听', artist='邓小平', album='K.B.Z Collection 80s-90s', duration=10, cover_url=urls[3], id='03'},
}
R.songs_i = 0

P = {}

P.list = {
	{},
	{},
	{},
	{},
	{},
}

P.i = 0

-- return song or nil
P.next = function ()
	P.i = P.i + 1
end

P.prev = function ()
	P.i = P.i - 1
end

P.cur = function () 
	return P.list[(P.i+1)%table.maxn(P.list)]
end

localmusic = P



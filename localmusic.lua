
local P

P = {}

P.list = {
	{title='Bad Attitude', artist='Lisa Germano', album='Happiness', 
		url='testalbum/1.mp3',
		--url='testdata/2s-1.mp3',
		cover_url='http://img.xiami.net/images/album/img43/13843/114457.jpg', id='01'},

	{title='The Thief', artist='Sarah Harmer', album='Oh Little Fire', 
		url='testalbum/2.mp3',
		--url='testdata/2s-1.mp3',
		cover_url='http://img.xiami.net/images/album/img49/28349/3864401277259763.jpg', id='02'},

	{title='Sleep Song 1', artist='K.B.Z', album='Best Of K.B.Z', 
		--url='testdata/2s-1.mp3',
		url='testalbum/3.mp3',
		cover_url='http://img.xiami.net/images/album/img94/10594/572641372578067.jpg', id='03'},

	{title='Sleep Song 2', artist='K.B.Z', album='Best Of K.B.Z', 
		url='testdata/2s-1.mp3',
		--url='testalbum/4.mp3',
		cover_url='http://sfault-avatar.b0.upaiyun.com/143/624/1436242287-1030000000158255_huge128', id='04'},
}

P.i = 0

-- return song or nil
P.next = function ()
	P.i = P.i + 1
	return P.cursong()
end

P.prev = function ()
	P.i = P.i - 1
	return P.cursong()
end

P.cursong = function () 
	return P.list[(P.i%table.maxn(P.list))+1]
end

P.info = function ()
	return {type='local'}
end

localmusic = P


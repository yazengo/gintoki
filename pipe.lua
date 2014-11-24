
pipe = {}

pipe.copy = function (...)
	local r = pcopy(...)

	r.done = function (cb)
		r.done_cb = cb
		return r
	end 

	return r
end

pipe.readall = function (p, done) 
	if p[1] then p = p[1] end
	local ss = strsink()

	ss.done_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss)
end

pipe.grep = function (p, word, done)
	if p[1] then p = p[1] end
	local ss = strsink('grep', word)

	ss.grep_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss)
end


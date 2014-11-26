
local R = {}

R.urls_list = function (urls)
	local r = {}
	local o = urls

	r.urls = urls
	r.at = 1

	r.fetch = function (done)
		r.fetch_imm = set_immediate(function ()
			local n = table.maxn(r.urls) 

			if r.at > n then 
				if o and o.loop then
					r.at = 1
				else
					info('ends')
					if r.done_cb then r.done_cb() end
					return 
				end
			end

			done{url=r.urls[r.at]}
			r.at = r.at + 1
			r.fetch_imm = nil
		end)
	end

	r.cancel_fetch = function ()
		if r.fetch_imm then
			cancel_immediate(r.fetch_imm)
			r.fetch_imm = nil
		end
	end

	r.done = function (cb)
		r.done_cb = cb
	end

	return r
end

R.new_station = function (S)
	local task
	local songs = {}
	local songs_i = 1

	local function new_task()
	end

	S.fetch = function (done)
		local s = songs[songs_i]
		if s then
			set_immediate(function () done(s) end)
			songs_i = songs_i + 1
		else
		end
	end

	S.new_task = function (o)
		if task then task.cancelled = true end
		task = {}
		task.fail = function (...)
		end
		task.done = function ()
		end
	end
end

radio = R


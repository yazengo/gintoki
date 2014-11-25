
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

	return r
end

radio = R


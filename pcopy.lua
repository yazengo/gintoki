
function pcopy(...)
	local r = _pcopy(...)

	r.done = function (cb)
		r.done_cb = cb
		return r
	end 

	return r
end


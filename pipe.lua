
apipe = function (...)
	local nodes = {...}
	local n = table.maxn(nodes)
	local r = {}

	if n <= 1 then
		panic('at least 2 nodes')
	end

	r.refcnt = 0

	if nodes[1].stdin then
		r.stdin = nodes[1].stdin
		r.refcnt = r.refcnt + 1
	end

	if nodes[n].stdout then
		r.stdout = nodes[n].stdout
		r.refcnt = r.refcnt + 1
	end

	for i = 1, n-1 do
		local src = nodes[i].stdout
		local sink = nodes[i+1].stdin
		if not src or not sink then
			panic('must specify src and sink')
		end
		pcopy(src, sink)
	end

	info(r.refcnt)

	return r
end


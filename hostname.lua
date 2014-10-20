
if hostplat() == 'mips' then
	local fp = io.open('macaddr', 'r')
	local mac = fp:read('*a')
	fp:close()

	local name = string.sub(mac, -6)
	hostname = function ()
		return name
	end
end

local uuid = tonumber(string.sub(sha1_encode(hostname()), -8), 16)

hostuuid = function ()
	return uuid
end


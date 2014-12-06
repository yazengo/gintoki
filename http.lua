
require('pipe')

http = {}

local retjson = function (json)
    local echocmd = string.format('echo "HTTP/1.1 200 OK\r\n\z
    Content-Length: %d; charset=utf-8\r\n\z
    Content-Type: text/html\r\n\r\n%s"', #json - 1, json)
    return pexec(echocmd, 'r')
end

-- req: tin > (hin > hout)
-- res: tin > (hin > resout) > tout
http.itunes = function (addr, port)
    tcpsrv(addr, port, function (tin, tout)
        local upload_dir = jz_itunes_dir or 'www/'
        local tempfile = upload_dir .. randomhexstring(8)
        h, hin, hout = http_server(
        function (req, res)
            local url = http_setopt(h, 'geturl') 
            local method = http_setopt(h, 'getmethod')
            info('iTunes url: ' .. url)
            info('iTunes method: ' .. method) 
            if url == '/upload' then
                if method == 1 then
                    pipe.copy(pexec('cat www/upload.html', 'r'), tout, 'rw')
                elseif method == 3 then
                    local filename = upload_dir .. http_setopt(h, 'getheader')
                    pexec('mv ' .. tempfile .. ' ' .. filename)
                    pipe.copy(retjson(cjson.encode{result = 0}), tout, 'rw').done(
                    function ()
                        info("iTunes save file:", filename)
                    end)
                end
            end
        end)

        http_setopt(h, 'setheader', 'Filename')
        pipe.copy(tin, hin, 'rw')
        pipe.copy(hout, pexec('cat > ' .. tempfile, 'w'), 'rw')
    end)
end



require('prop')

local T = {}

T.download = function (words, done, o)
	o = o or {}

	local filepath = string.gsub(words, ' ', '-') .. '.mp3'
	local path = (tts_files_root or 'testaudios/') .. filepath

	if pathexists(path) then
		done(path)
		return
	end

	local url = 'http://translate.google.com/translate_tts?' .. encode_params {
		tl = o.lang or 'en',
		ie = 'UTF-8',
		q = words,
	}
	info(url)

	curl {
		url = url,
		proxy = prop.get('gfw.exist', true) and 'sugrsugr.com:8889',
		retfile = path,
		done = function (r, st)
			if st.stat == 'done' then
				done(path)
			else
				os.remove(path)
			end
		end,
	}
end

tts = T


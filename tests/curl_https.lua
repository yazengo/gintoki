
setloglevel(0)

curl {
	url = 'https://kernel.org',
	done = function (ret, stat)
		info(stat)
	end,
}


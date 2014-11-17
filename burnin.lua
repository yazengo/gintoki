
require('prop')

local B = {}

B.setopt = function (o, done)
	if o.op == 'burnin.start' then
		B.burnin_tm = now()
		B.que = ctrl.breaking_audio('noise://', {resume=true})
		done{result=0}
		return true
	elseif o.op == 'burnin.stop' then
		if B.burnin_tm then
			local elapsed = now() - B.burnin_tm
			local total = prop.get('burnin.time', 0) + elapsed
			prop.set('burnin.time', total)
		end
		if B.que then
			B.que.stop()
			B.que = nil
		end
		done{result=0}
		return true
	elseif o.op == 'burnin.totaltime' then
		done{result=math.ceil(prop.get('burnin.time', 0))}
		return true
	end
end

burnin = B


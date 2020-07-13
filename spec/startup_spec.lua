--[[
test startup of nanomsg api
]]

describe("nng",function()
	local nng
	it("should be included with require()",function()
		nng = require("nng")
	end)
	it("should be able to create sockets",function()
		local socket = assert(nng.pair1_open())
	end)
	it("should be able to extablish a connection over inter-process communication",function()
		local s1 = assert(nng.pair1_open())
		local s2 = assert(nng.pair1_open())
		assert(s1:listen("ipc:///tmp/pair.ipc"))
		assert(s2:dial("ipc://tmp/pair.ipc"))
		
		assert(s2:send("hello"))
		local rec = assert(s1:recv())
		assert(rec == "hello","Failed to receive hello, received:" .. rec)
	end)
	it("should be able to use a bus socket to distribute information",function()
		local b = {}
		for i = 1,10 do
			local s = assert(nng.bus0_open())
			b[i] = s
		end
		for i = 1,10 do
			local ipcaddr = string.format("ipc:///tmp/bus_%d.ipc",i)
			assert(b[i]:listen(ipcaddr))
		end
		for i = 1,10 do
			for j = 1,10 do
				if i ~= j then
					local addr = string.format("ipc:///tmp/bus_%d.ipc",j)
					assert(b[i]:dial(addr))
				end
			end
		end
		assert(b[1]:send("Hello"))
		for i = 2,10 do
			local msg = assert(b[i]:recv())
			assert(msg == "Hello")
		end
	end)
	it("should be able to use a survey socket to gather information",function()
		math.randomseed(os.time())
		local s = assert(nng.surveyor0_open())
		assert(s:listen("ipc:///tmp/survey.ipc"))
		local b = {}
		for i = 1,100 do
			local r = assert(nng.respondent0_open())
			assert(r:dial("ipc:///tmp/survey.ipc"))
			b[i] = r
		end
		assert(s:send("Hello"))
		for i = 1,100 do
			local survey = assert(b[i]:recv())
			assert(survey == "Hello")
			assert(b[i]:send(string.format("%f",math.random())))
		end
		local responses = {}
		while true do
			local succ, msg = s:recv(nng.NNG_FLAG_NONBLOCK)
			if succ then
				table.insert(responses,tonumber(succ))
			elseif msg == "Try again" then
				os.execute("sleep 1")
			elseif msg == "Incorrect state" then
				break
			end
		end
		local avg = 0
		for _,v in pairs(responses) do
			avg = avg + v
		end
		avg = avg / #responses
		--avg should be about 0.5
		assert(avg > 0.4)
		assert(avg < 0.6)
	end)
end)

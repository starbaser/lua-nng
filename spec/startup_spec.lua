--[[
test startup of the nng api
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
		print("Starting survayor respondent test")
		math.randomseed(os.time())
		local s = assert(nng.surveyor0_open())
		print("About to listen")
		assert(s:listen("ipc:///tmp/survey.ipc"))
		print("S is listening")
		local b = {}
		for i = 1,100 do
			print("Testing",i)
			local r = assert(nng.respondent0_open())
			assert(r:dial("ipc:///tmp/survey.ipc"))
			print("Dialed",i)
			b[i] = r
		end
		print("About to send hello")
		assert(s:send("Hello"))
		print("Done sending hello")
		for i = 1,100 do
			print("About to recv from", i)
			local survey = assert(b[i]:recv())
			print("Done receving from ",i)
			assert(survey == "Hello")
			print("About to send number back")
			assert(b[i]:send(string.format("%f",math.random())))
			print("Done sending number back")
		end
		local responses = {}
		while true do
			print("Got ", #responses, "responses")
			local succ, msg = s:recv(nng.NNG_FLAG_NONBLOCK)
			if succ then
				table.insert(responses,tonumber(succ))
			elseif msg == "Try again" then
				print("Sleeping...")
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
		assert(avg > 0.4, "Average was:" .. avg)
		assert(avg < 0.6, "Average was:" .. avg)
		print("Completed survayor respondent test")
	end)
	it("should be able to use publish and subscribe sockets to transfer information", function()
		print("starting pubsub test")
		--for i = 1,1000 do
		for i = 1,10 do
			local s1 = assert(nng.pub0_open())
			local s2 = assert(nng.sub0_open())
			local s3 = assert(nng.sub0_open())
			print("everything opened")
			--local listener, err = s1:listen("tcp://127.0.0.1:1000")
			local listener, err = s1:listen("ipc:///tmp/pub.ipc")
			local dialers = {}
			local num_addr_in_use = 0
			while err == "Address in use" do
				print("Got addr in use")
				num_addr_in_use = num_addr_in_use + 1
				if num_addr_in_use > 10 then
					error("After multiple attempts, failed to bind on round " .. i)
				end
				listener, err = s1:listen("tcp://127.0.0.1:1000")
				--listener, err = s1:listen("ipc:///tmp/pub.ipc")
			end
			print("s1 is listeneing...")
			assert(s2:dial("tcp://127.0.0.1:1000"))
			assert(s3:dial("tcp://127.0.0.1:1000"))
			--local d1 = assert(s2:dial("ipc:///tmp/pub.ipc"))
			--local d2 = assert(s3:dial("ipc:///tmp/pub.ipc"))
			assert(s2:subscribe(""))
			assert(s3:subscribe(""))
			table.insert(dialers, d1)
			table.insert(dialers, d2)
			print("Everything set up")
			assert(s1:send("hello 1"))
			print("sent")
			local r1 = assert(s2:recv())
			assert.are_equal(r1,"hello 1")
			print("received 1")
			local r2 = assert(s2:recv())
			assert.are_equal(s2,"hello 1")
			print("received 2")
			listener:close()
			s1:close()
			s2:close()
			s3:close()
		end
		print("Finishing pubsub test")
	end)
	describe("socket option",function()
		describe("LOCADDR",function()
			it("should return the local address for a socket",function()
				print("loc addr test")
				local nng = require("nng")
				print("open1")
				local s1 = assert(nng.bus0_open())
				print("open2")
				local s2 = assert(nng.bus0_open())
				print("3")
				local listener = assert(s1:listen("tcp://127.0.0.1:1001"))
				print("4")
				local dialer = assert(s2:dial("tcp://127.0.0.1:1001"))
				--local listener = s1:listen("ipc:///tmp/locaddr.ipc")
				--local dialer = s2:dial("ipc:///tmp/locaddr.ipc")
				print("5")
				assert(s1:send("test"))
				assert(s2:recv() == "test")
				print("s1:",s1)
				print("s2:",s2)
				local addr = dialer[nng.NNG_OPT_URL]
				print("addr:",addr)
				print("family:",addr.type)
				print("locaddr:",addr.addr)
			end)
			it("should be able to get the local ip address from google #writing",function()
				print("rem addr test")
				local nng = require("nng")
				local s1 = assert(nng.req0_open())
				assert(s1:dial("tcp://www.ipchicken.com:80"))
				assert(s1:send(""))
				local data = assert(s1:recv())
				print("data:",data)
			end)
		end)
	end)
	describe("tcp transport",function()
		it("has a keepalive option that prevents the tcp connection from closing #writing2",function()
			local nng = require("nng")
			print("starting tcp transport test")
			local s1 = assert(nng.pair1_open())
			local s2 = assert(nng.pair1_open())
			s1[nng.NNG_OPT_TCP_KEEPALIVE] = true
			--print(s1[nng.NNG_OPT_TCP_KEEPALIVE])
			assert(s1:listen("tcp://127.0.0.1:1000"))
			assert(s2:dial("tcp://127.0.0.1:1000"))
			print("Finished tcp transport test")
		end)
	end)
end)

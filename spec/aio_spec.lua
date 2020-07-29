--[[
test the asyncronous parts of nnng
]]
local nng = require("nng")
local lanes = require("lanes").configure()
describe("nng.aio",function()
	it("should be able to create aio object",function()
		print("one")
		local aio = assert(nng.aio.alloc(function() end))
	end)
	it("should accept a callback with arguments",function()
		print("two")
		local aio = assert(nng.aio.alloc(function(a,b,c)
			print("hello")
		end, "one", 2, "three"))
	end)
	--it("should call the callback after sleeping", function()
		--print("three")
		--local called = false
		--local callback = function(lock)
			--called = true
		--end
		--print("about to alloc")
		--local aio = assert(nng.aio.alloc(callback))
		--print("done alloc")
		--print("about to sleep")
		--nng.aio.sleep(1,aio)
		--print("whatever:",whatever)
		----nng.aio.sleep(1,aio)
		--print("done sleep")
		--os.execute("sleep 1")
		--lock(-1)
		--print("checking called...")
		--print("called was",called)
		--assert(called)
	--end)
	it("should call more than one socket getting a callback at once",function()
		print("checking recv_any callback")
		local s1 = assert(nng.bus0_open())
		assert(s1:listen("tcp://127.0.0.1:4000"))

		local s2 = assert(nng.bus0_open())
		assert(s2:listen("tcp://127.0.0.1:4001"))

		local s3 = assert(nng.bus0_open())
		assert(s3:dial("tcp://127.0.0.1:4000"))
		assert(s3:dial("tcp://127.0.0.1:4001"))

		for i = 1, 100 do --100 times to try to trigger race conditions
			--print("i:",i)
			assert(s3:send("one"))
			assert(s3:send("two"))
			local s1_got_one, s1_got_two, s2_got_one, s2_got_two = false, false, false, false
			while not (s1_got_one and s1_got_two and s2_got_one and s2_got_two) do
				--local socket, message = nng.aio.recv_any(s1, s2)
				--print("about to start recv_any")
				--local tbl = nng.aio.recv_any(s1,s2)
				for socket, message in pairs(nng.aio.recv_any(s1,s2)) do
					--print("in one recv any:",socket, message)
					if socket == s1 then
						if message == "one" then
							s1_got_one = true
						elseif message == "two" then
							s1_got_two = true
						else
							error("message socket 1:" .. message)
						end
					elseif socket == s2 then
						if message == "one" then
							s2_got_one = true
						elseif message == "two" then
							s2_got_two = true
						else
							error("message socket 2:" .. message)
						end
					else
						error("socket:" .. tostring(socket))
					end
					--print("done with recv_any",s1_got_one, s1_got_two, s2_got_one, s2_got_two)
				end
			end
			assert(s1_got_one)
			assert(s1_got_two)
			assert(s2_got_one)
			assert(s2_got_two)
		end
		print("Sucessful completion of recv_any test")
	end)
	it("Should accept multiple sockets of dispirate types #writing",function()
		local servers = {}
		local clients = {}
		local messages = {}
		local halfs = {
			{"rep","req",true},
			{"bus","bus",nil},
			{"surveyor","respondent",false},
			{"pub","sub",false}
		}
		for i = 1, 10 do
			local rng = math.random(#halfs)

			--Create server
			servers[i] = assert(nng[halfs[rng][1] .. "0_open"]())
			local url = string.format("tcp://127.0.0.1:%d",4000 + i)
			assert(servers[i]:listen(url))

			--Create clients
			local numclients = math.random(1,3)
			clients[i] = {}
			messages[i] = {}
			for j = 1, numclients do
				clients[i][j] = assert(nng[halfs[rng][2] .. "0_open"]())
				assert(clients[i][j]:dial(url))
				messages[i][j] = {}
				if halfs[rng][3] == true then
					--we send messages
					local nummessages = math.random(5)
					for k = 1, nummessages do
						local message = string.format("ping_%d_%d_%d",i,j,k)
						assert(clients[i][j]:send(message))
						table.insert(messages[i][j],message)
					end
				--elseif halfs[rng][3] == nil then
					--we can send or receive messages
					
				else
					--we receive and reply to messages

				end
			end
		end
		for socket, message in pairs(nng.aio.recv_any(table.unpack(servers))) do
			print("GOT MESSAGE:",message)
		end
	end)
end)

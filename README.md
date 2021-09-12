# lua-nng

This is a simple binding of [Nanomessage Next Generation](https://github.com/nanomsg/nng) to lua.

## Installation

First you'll need a copy of [nng](https://github.com/nanomsg/nng)

	git clone https://github.com/nanomsg/nng
	cd nng
	mkdir build
	cd build
	cmake .. -DBUILD_SHARED_LIBS=True
	make && sudo make install


The easiest way to download lua-nng is with [luarocks](https://github.com/luarocks/luarocks).

You can also clone this repository and build locally

	git clone https://cogarr.net/source/cgit.cgi/lua-nng
	cd lua-nng
	sudo luarocks build


```
luarocks install --server=http://rocks.cogarr.net lua-nng
```

## Example

	local nng = require("nng")

	local s1 = nng.pair1_open()
	local s2 = nng.pair1_open()

	s1:listen("ipc:///tmp/pair.ipc")
	s2:dial("ipc:///tmp/pair.ipc")

	s2:send("hello")
	print(s1:recv()) --prints "hello"

For more examples, see spec/start\_spec.lua


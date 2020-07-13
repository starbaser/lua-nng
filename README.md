# lua-nng

This is a simple binding of [Nanomessage Next Generation](https://github.com/nanomsg/nng) to lua.

## Installation

The easiest way to download lua-nng is with [luarocks](https://github.com/luarocks/luarocks)

```
luarocks install --server=http://rocks.cogarr.net lua-nng
```

## Example

	local s1 = nng.pair1_open()
	local s2 = nng.pair1_open()

	s1:listen("ipc:///tmp/pair.ipc")
	s2:dial("ipc://tmp/pair.ipc")

	s2:send("hello")
	print(s1:recv())

prints "hello"

For more examples, see sepc/start\_spec.lua


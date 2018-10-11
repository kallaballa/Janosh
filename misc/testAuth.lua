#!/usr/local/bin/janosh -f

function echo(handle, message)
  -- echo message back
  Janosh:wsSend(handle, message)
end

-- open websocket
Janosh:wsOpen(10000,"passwd.txt")
Janosh:wsOnReceive(echo)

Janosh:forever()

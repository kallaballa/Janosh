#!/Users/j/Documents/bin/janosh -f
-- enables short global function names for the janosh api

local bla="im global"

-- runs when /val is changed
function sub(key,op,value) 
  -- update all clients with whole database. do it in a transaction
  print(key,op,value)
end

function say(key,op,value)
  print("say: " .. value)
end

function sub1(key,op,value)
  -- update all clients with whole database. do it in a transaction
	janosh_get({"/."})
  Janosh:wsBroadcast(key .. " " .. op .. " " .. value);
end

-- runs when a websocket message is received
function echo(handle, message)
  -- echo message back
  print(message)
  -- Janosh:wsSend(handle, message)
end


table.foreach(_G,print)


-- init the db and /val
print("Yo:A")
Janosh:truncate()
print("Yo:B")

print("Yo:C")
Janosh:set("/val", "1")
print("Yo:D")

-- open websocket
Janosh:wsOpen(9998)

-- install subscription callback
Janosh:subscribe("/", sub)
--Jsubscribe("/val", sub)
Janosh:subscribe("say", say)
-- install receive callback
Janosh:wsOnReceive(echo)

print("loop")

while true do
  -- set, remove and set /val. do it in a transaction so that everybody else sees /val as 1
  Janosh:transaction(function()
	  Janosh:set("/val", "0")
  	Janosh:remove("/val")
		Janosh:sleep(1000)
	  Janosh:trigger("/val","1")
    Janosh:publish("say", "W", "hi")
  end)
end

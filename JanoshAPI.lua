#!/usr/local/bin/lua

if __JanoshFirstStart then
lanes = require "lanes".configure{ on_state_create = janosh_install ;}
end

require "zmq"
io = require "io"

local JanoshClass = {} -- the table representing the class, which will double as the metatable for the instances
JanoshClass.__index = JanoshClass -- failed table lookups on the instances should fallback to the class table, to get methods

function JanoshClass.new()
  return setmetatable({}, JanoshClass)
end

function JanoshClass.request(self, req) 
  return janosh_request(req);
end

function JanoshClass.set(self, key, value)
	janosh_set({key,value});
end

function JanoshClass.set_all(self, argv)
  janosh_set(argv);
end

function JanoshClass.add(self, key, value)
  janosh_add({key,value});
end

function JanoshClass.add_all(self, argv)
  janosh_add(argv);
end

function JanoshClass.trigger(self, key, value)
  janosh_trigger({key,value});
end

function JanoshClass.trigger_all(self, argv)
  janosh_trigger(argv);
end

function JanoshClass.replace(self, key, value)
  janosh_replace({key,value});
end

function JanoshClass.replace_all(self, argv)
  janosh_replace(argv);
end

function JanoshClass.append(self, key, value)
  if type(value) == "table" then
		table.insert(value, 1, key)
    janosh_append(value);
	else
		janosh_append({key,value})
	end
end

function JanoshClass.dump(self)
   return janosh_dump({})
end

function JanoshClass.size(self, keys)
  if type(keys) == "table" then
    return janosh_size(keys);
  else
    return janosh_size({keys})
  end
end

function JanoshClass.get(self, keys)
  if type(keys) == "table" then
    return JSON:decode(janosh_get(keys));
  else
    return JSON:decode(janosh_get({keys}))
  end
end
  
function JanoshClass.copy(self, from, to)
	janosh_copy({from,to})
end

function JanoshClass.remove(self, keys)
  if type(keys) == "table" then
    janosh_remove(keys);
  else
    janosh_remove({keys})
  end
end

function JanoshClass.shift(self, from, to)
  janosh_shift({from,to})
end

function JanoshClass.move(self, from, to)
  janosh_move({from,to})
end

function JanoshClass.truncate(self)
  janosh_truncate({})
end

function JanoshClass.mkarr(self, keys)
  if type(keys) == "table" then
    janosh_mkarr(keys);
  else
    janosh_mkarr({keys})
  end
end

function JanoshClass.mkobj(self, keys)
  if type(keys) == "table" then
    janosh_mkobj(keys);
  else
    janosh_mkobj({keys})
  end
end

function JanoshClass.hash(self)
  janosh_hash({})
end

function JanoshClass.open(self)
  janosh_open({})
end

function JanoshClass.close(self)
  janosh_close({})
end

function JanoshClass.transaction(self, fn) 
  self:open({})
  fn()
  self:close({})
end

function JanoshClass.subscribe(self, keyprefix, callback)
  janosh_subscribe(keyprefix);
  lanes.gen("*", function() 
		janosh_register_thread("Subscriber")
		while true do
			key, op, value = janosh_receive(keyprefix)
			callback(key, op, value)
		end
	end)()
end

function JanoshClass.wsBroadcast(self, msg) 
  janosh_wsbroadcast(msg)
end

function JanoshClass.wsOpen(self, port)
 janosh_wsopen(port)
end


function JanoshClass.subscribe(self, keyprefix, callback)
  janosh_subscribe(keyprefix);
  lanes.gen("*", function()
    janosh_register_thread("Subscriber")
    while true do
      key, op, value = janosh_receive(keyprefix)
      callback(key, op, value)
    end
  end)()
end

function JanoshClass.wsOnReceive(self, callback) 
  lanes.gen("*", function()
    janosh_register_thread("WsReceiver")
    while true do
      key, op, value = janosh_wsreceive(keyprefix)
      callback(key, op, value)
    end
  end)()
end

function JanoshClass.wsSend(self, handle, msg) 
 janosh_wssend(handle,msg)
end

function JanoshClass.sleep(self, millis)
 janosh_sleep(millis)
end

function JanoshClass.lock(self, name) 
	janosh_lock(name)
end

function JanoshClass.unlock(self, name)
  janosh_unlock(name)
end

function JanoshClass.exec(self, commands) 
  for i, cmd in ipairs(commands) do
	  os.execute(cmd);
	end
end

function JanoshClass.popenr(self,cmd)
  return io.popen(cmd, "r")
end

function JanoshClass.popenw(self,cmd)
  return io.popen(cmd, "w")
end


function setfield (f, v)
	local t = _G    -- start with the table of globals
	for w, d in string.gfind(f, "([%w_]+)(.?)") do
		if d == "." then      -- not last field?
			t[w] = t[w] or {}   -- create table if absent
			t = t[w]            -- get the table
		else                  -- last field
			t[w] = v            -- do the assignment
		end
	end
end

function JanoshClass.shorthand(self) 
for key,value in pairs(getmetatable(self)) do
  setfield("J" .. key, function(...) return value(self,...) end)
end
end

return JanoshClass:new()

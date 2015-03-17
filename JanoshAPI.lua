#!/usr/local/bin/janosh -f

require"zmq"
require"zmq.threads"

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

function JanoshClass.subscribe(self, keyprefix, callback)
  binary = string.dump(callback);
	formatted_binary = ""
	for i = 1, string.len(binary) do
		dec, _ = ("\\%3d"):format(binary:sub(i, i):byte()):gsub(' ', '0')
		formatted_binary = formatted_binary .. dec
	end
  janosh_subscribe(keyprefix, formatted_binary);
end

return JanoshClass:new()

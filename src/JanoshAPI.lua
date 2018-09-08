#!/usr/local/bin/luajit

if __JanoshFirstStart then
  lanes = require "lanes".configure{ on_state_create = janosh_install; verbose_errors = true; protect_allocator = true; with_timers = false; }
end
local posix = require "posix"
local bit = require "bit"

io = require "io"

local JanoshClass = {} -- the table representing the class, which will double as the metatable for the instances
JanoshClass.__index = JanoshClass -- failed table lookups on the instances should fallback to the class table, to get methods

function JanoshClass.new()
  return setmetatable({}, JanoshClass)
end

function JanoshClass.basename(self, path)
  local name = string.gsub(path, "(.*/)(.*)", "%2")
  return name
end

function JanoshClass.epoch(self)
  return posix.time()
end

function JanoshClass.setenv(self,key,value)
	posix.setenv(key,value)
end

function JanoshClass.mktemp(self)
  return Janosh:capture("mktemp"):gsub("^%s*(.-)%s*$", "%1")  
end

function JanoshClass.mktempDir(self)
  return Janosh:capture("mktemp -d"):gsub("^%s*(.-)%s*$", "%1")
end

function JanoshClass.system(self, cmdstring) 
 os.execute(cmdstring)
end

function JanoshClass.capture(self, cmd)
  local f = assert(io.popen(cmd, 'r'))
  local s = assert(f:read('*a'))
  f:close()

  return s
end

--FIXME causes wierd errors
function JanoshClass.unsetenv(self,key)
  assert(false)
	posix.unsetenv(key)
end

function JanoshClass.fopen(self,path,flags,mode) 
	return posix.open(path,mode)
end

function JanoshClass.fopenr(self,path)
  return posix.open(path, bit.bor(posix.O_RDONLY))
end

function JanoshClass.fopenw(self,path)
  return posix.open(path, bit.bor(posix.O_WRONLY))
end

function JanoshClass.fwrite(self, fd, str)
  return posix.write(fd,str)
end

function JanoshClass.fwriteln(self, fd, str)
  return posix.write(fd,str .. "\n")
end


function JanoshClass.fclose(self, fd)
  posix.close(fd)
end

local b='ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/' -- You will need this for encoding/decoding
-- encoding
function JanoshClass.enc64(self,data)
    return ((data:gsub('.', function(x) 
        local r,b='',x:byte()
        for i=8,1,-1 do r=r..(b%2^i-b%2^(i-1)>0 and '1' or '0') end
        return r;
    end)..'0000'):gsub('%d%d%d?%d?%d?%d?', function(x)
        if (#x < 6) then return '' end
        local c=0
        for i=1,6 do c=c+(x:sub(i,i)=='1' and 2^(6-i) or 0) end
        return b:sub(c+1,c+1)
    end)..({ '', '==', '=' })[#data%3+1])
end

-- decoding
function JanoshClass.dec64(self, data)
    data = string.gsub(data, '[^'..b..'=]', '')
    return (data:gsub('.', function(x)
        if (x == '=') then return '' end
        local r,f='',(b:find(x)-1)
        for i=6,1,-1 do r=r..(f%2^i-f%2^(i-1)>0 and '1' or '0') end
        return r;
    end):gsub('%d%d%d?%d?%d?%d?%d?%d?', function(x)
        if (#x ~= 8) then return '' end
        local c=0
        for i=1,8 do c=c+(x:sub(i,i)=='1' and 2^(8-i) or 0) end
            return string.char(c)
    end))
end

function JanoshClass.tprint(self, tbl, indent)
  if not indent then indent = 0 end
  for k, v in pairs(tbl) do
    formatting = string.rep("  ", indent) .. k .. ": "
    if type(v) == "table" then
      print(formatting)
      self:tprint(v, indent+1)
    elseif type(v) == 'boolean' then
      print(formatting .. tostring(v))
    else
      print(formatting .. v)
    end
  end
end

function JanoshClass.pwrite(self, fd, string)
	posix.write(fd,string)
end

function JanoshClass.pclose(self, fd) 
	posix.close(fd)
end

function JanoshClass.pwait(self, fd)
  posix.wait(fd)
end

function JanoshClass.pread(self, fd, num) 
	return posix.read(fd,num)
end

function JanoshClass.preadLine(self,fd)
  line="";
 	while true do
		char = self:pread(fd,1)
		if char == nil or #char == 0 then
			return nil
		end
    if char == "\n" then break end
		line = line .. char
	end
  return line;
end

function JanoshClass.psystem(self,cmd)
  return Janosh:popen("bash", "-c", "exec " .. cmd)
end

function JanoshClass.popen(self, path, ...)
    local r1, w1 = posix.pipe()
    local r2, w2 = posix.pipe()
    local r3, w3 = posix.pipe()

    assert((w1 ~= nil or r2 ~= nil or r3 ~= nil), "pipe() failed")

    local pid, err = posix.fork()
    assert(pid ~= nil, "fork() failed")
    if pid == 0 then
        posix.close(w1)
        posix.close(r2)
        posix.dup2(r1, posix.fileno(io.stdin))
        posix.dup2(w2, posix.fileno(io.stdout))
        posix.dup2(w3, posix.fileno(io.stderr))
        posix.close(r1)
        posix.close(w2)
        posix.close(w3)

        local ret, err = posix.execp(path, unpack({...}))
        assert(ret ~= nil, "execp() failed: " .. path)

        posix._exit(1)
        return
    end

    posix.close(r1)
    posix.close(w2)
    posix.close(w3)

    return pid, w1, r2, r3
end

--
-- Pipe input into cmd + optional arguments and wait for completion
-- and then return status code, stdout and stderr from cmd.
--
function JanoshClass.pipe_simple(self, input, cmd, ...)
    --
    -- Launch child process
    --
    local pid, w, r, e = self:popen(cmd, unpack({...}))
    assert(pid ~= nil, "filter() unable to popen3()")

    --
    -- Write to popen3's stdin, important to close it as some (most?) proccess
    -- block until the stdin pipe is closed
    --
    posix.write(w, input)
    posix.close(w)

    local bufsize = 4096
    --
    -- Read popen3's stdout via Posix file handle
    --
    local stdout = {}
    local i = 1
    while true do
        buf = posix.read(r, bufsize)
        if buf == nil or #buf == 0 then break end
        stdout[i] = buf
        i = i + 1
    end

    --
    -- Read popen3's stderr via Posix file handle
    --
    local stderr = {}
    local i = 1
    while true do
        buf = posix.read(e, bufsize)
        if buf == nil or #buf == 0 then break end
        stderr[i] = buf
        i = i + 1
    end

    --
    -- Clean-up child (no zombies) and get return status
    --
    local wait_pid, wait_cause, wait_status = posix.wait(pid)

    return wait_status, table.concat(stdout), table.concat(stderr)
end

function JanoshClass.thread(self, fn) 
  return lanes.gen("*", fn)
end

function JanoshClass.tjoin(self, t)
  return t[1]
end

function JanoshClass.request(self, req) 
  table.insert(req,1,debug.traceback())
  return janosh_request(req)
end

function JanoshClass.request_t(self, req)
  table.insert(req,1,debug.traceback())
  return janosh_request_t(req)
end

function JanoshClass.load(self, value)
  local ret, value = self:request({"load",JSON:encode(value)})
  return ret
end


function JanoshClass.set(self, key, value)
  local ret, val = self:request({"set",key,value})
  return ret
end

function JanoshClass.set_all(self, argv)
  table.insert(argv,1,"set")
  local ret, val = self:request(argv)
  return ret
end

function JanoshClass.add(self, key, value)
  local ret, val = self:request({"add",key,value})
  return ret
end

function JanoshClass.add_all(self, argv)
  table.insert(argv,1,"add")
  local ret, val = self:request(argv);
  return ret;
end

function JanoshClass.replace(self, key, value)
  local ret, val = self:request({"replace",key,value});
  return ret;
end

function JanoshClass.replace_all(self, argv)
  table.insert(argv,1,"replace")
  local ret, val = self:request(argv);
  return ret
end

function JanoshClass.append(self, key, value)
  if type(value) == "table" then
    table.insert(value, 1, key)
    table.insert(value, 1, "append")
    local ret, val = self:request(value); 
      return ret
    else
    local ret, val = self:request({"append",key,value})
      return ret
  end
end

function JanoshClass.dump(self)
    local err, value = self:request({"dump"})
    if not err then
      return nil
    end

   return value
end

function JanoshClass.size(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "size")
     local err, value = self:request(keys)
    if not err then
      return nil
    end

    return tonumber(value)
  else
	local err, value = self:request({"size", keys })
    if not err then
      return nil
    end

    return tonumber(value)
  end
end

function countNonTable(tbl)
	local cnt = 0
	for k, v in pairs(tbl) do
		if type(v) ~= "table" then
			cnt = cnt + 1
		else
			cnt = cnt + countNonTable(v)
		end
	end
	return cnt
end

function ends(String,End)
   return End=='' or string.sub(String,-string.len(End))==End
end

function getFirstLeaf(tbl)
  for k, v in pairs(tbl) do
    if type(v) ~= "table" then
			return v;
    else
			return getFirstLeaf(v)
    end
  end
end

function JanoshClass.get(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "get")
		local err, value = self:request(keys)
		if not err then
			return nil
		end
    local table = JSON:decode(value)
		local hasDir = false;
		for k, v in pairs(keys) do
			hasDir = ends(k,"/.")
			if hasDir then
				break;
			end
		end

		if not hasDir then
			if countNonTable(table) == 1 then
				return getFirstLeaf(table)
			else
				return table;
			end
		else
			return table;
		end	
	else
    local err, value = self:request({"get", keys})
    if not err then
      return nil
    end
    local table = JSON:decode(value)

		if ends(keys, "/.") then
			return table
		else
			if countNonTable(table) == 1 then
      	return getFirstLeaf(table)
    	else
  	    return table;
	    end
		end
  end
end
  
function JanoshClass.copy(self, from, to)
  local ret, val = self:request({"copy",from,to})
  return ret;
end

function JanoshClass.remove(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "remove")
    local ret, val = self:request(keys);
    return ret;
  else
    local ret, val = self:request({"remove", keys})
    return ret;
  end
end

function JanoshClass.shift(self, from, to)
  local ret, val = self:request({"shift", from,to})
  return ret;
end

function JanoshClass.move(self, from, to)
  local ret, val = self:request({"move", from,to})
  return ret;
end

function JanoshClass.truncate(self)
  local ret, val = self:request({"truncate"})
  return ret;
end

function JanoshClass.mkarr(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "mkarr")
    local ret, val = self:request(keys);
    return ret;
  else
    local ret, val = self:request({"mkarr", keys})
    return ret;
  end
end

function JanoshClass.mkobj(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "mkobj")
    local ret, val = self:request(keys);
    return ret
  else
    local ret, val = self:request({"mkobj", keys})
    return ret
  end
end

function JanoshClass.hash(self)
  local ret, val = self:request({"hash"})
  return val;
end

function JanoshClass.open(self, strID)
  janosh_open(strID)
end

function JanoshClass.close(self)
  janosh_close()
end

function count(base, pattern)
	return select(2, string.gsub(base, pattern, ""))
end

function JanoshClass.transaction(self, fn) 
  if count(debug.traceback(), "%[string \"JanoshAPI\"%]: in function 'transaction'") > 1 then
    error("Nested transaction detected: " .. debug.traceback());
  end
  self:open(debug.traceback())
  status, msg = pcall(fn)
  if not status then
    print("Transaction failed: " .. msg)
  end

  self:close()
end

function JanoshClass.subscribe(self, keyprefix, callback)
  janosh_subscribe(keyprefix);
  t = lanes.gen("*", function() 
		janosh_register_thread("Subscriber: " .. keyprefix)
		while true do
			key, op, value = janosh_receive(keyprefix)
			status, msg = pcall(function() callback(key, op, value) end)
      if not status then
        print("Subscriber " .. keyprefix .. " failed: ", msg)
      end
		end
	end)
  t()
end

function JanoshClass.publish(self, key, op, value) 
  local ret, val = self:request({"publish",key,op,value});
  return ret
end

function JanoshClass.set_t(self, key, value)
  local ret, val = self:request_t({"set",key,value});
  return ret
end

function JanoshClass.set_all_t(self, argv)
  table.insert(argv,1,"set")
  local ret, val = self:request_t(argv);
  return ret;
end

function JanoshClass.add_t(self, key, value)
  local ret, val = self:request_t({"add",key,value});
  return ret
end

function JanoshClass.add_all_t(self, argv)
  table.insert(argv,1,"add")
  local ret, val = self:request_t(argv);
  return ret
end

function JanoshClass.replace_t(self, key, value)
  local ret, val = self:request_t({"replace",key,value});
  return ret;
end

function JanoshClass.replace_all_t(self, argv)
  table.insert(argv,1,"replace")
  local ret, val = self:request_t(argv);
  return ret
end

function JanoshClass.append_t(self, key, value)
  if type(value) == "table" then
    table.insert(value, 1, key)
    table.insert(value, 1, "append")
    local ret, val = self:request_t(value);
    return ret
  else
    local ret, val = self:request_t({"append",key,value})
    return ret
  end
end

function JanoshClass.copy_t(self, from, to)
  local ret, val = self:request_t({"copy",from,to})
  return ret
end

function JanoshClass.remove_t(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "remove")
    local ret, val = self:request_t(keys);
    return ret
  else
    local ret, val = self:request_t({"remove", keys})
    return ret
  end
end

function JanoshClass.shift_t(self, from, to)
  local ret, val = self:request_t({"shift", from,to})
  return ret
end

function JanoshClass.move(self, from, to)
  local ret, val = self:request({"move", from,to})
  return ret
end

function JanoshClass.move_t(self, from, to)
  local ret, val = self:request_t({"move", from,to})
  return ret
end

function JanoshClass.mkarr_t(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "mkarr")
    local ret, val = self:request_t(keys);
    return ret
  else
    local ret, val = self:request_t({"mkarr", keys})
    return ret
  end
end

function JanoshClass.mkobj_t(self, keys)
  if type(keys) == "table" then
    table.insert(keys, 1, "mkobj")
    local ret, val = self:request_t(keys);
    return ret
  else
    local ret, val = self:request_t({"mkobj", keys})
    return ret
  end
end

function JanoshClass.wsBroadcast(self, msg) 
  janosh_wsbroadcast(msg)
end

function JanoshClass.wsOpen(self, port)
 janosh_wsopen(port)
end

function JanoshClass.wsOnReceive(self, callback) 
  lanes.gen("*", function()
    janosh_register_thread("WsReceiver")
    while true do
      key, op, value = janosh_wsreceive(keyprefix)
      status, msg = pcall(callback,key, op, value)
      if not status then
        print("Receiver " .. keyprefix .. " failed: ", msg)
      end
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

function JanoshClass.try_lock(self, name)
  return janosh_try_lock(name)
end

function JanoshClass.unlock(self, name)
  janosh_unlock(name)
end

function JanoshClass.mouseMove(self, x, y)
  janosh_mouse_move(x,y)
end

function JanoshClass.mouseMoveRel(self, x, y)
  janosh_mouse_move_rel(x,y)
end

function JanoshClass.mouseUp(self, button)
  janosh_mouse_up(button)
end

function JanoshClass.mouseDown(self, button)
  janosh_mouse_down(button)
end

function JanoshClass.keyUp(self, keySym)
  janosh_key_up(keySym)
end

function JanoshClass.keyDown(self, keySym)
  janosh_key_down(keySym)
end

function JanoshClass.keyType(self, keySym)
  janosh_key_type(keySym)
end

function JanoshClass.bind(self, f, ...)
   return function() return f1(a) end
end

function JanoshClass.test(self, ...)
  local arg={...}
  mfunc = table.remove(arg,1);
--  JanoshClass.tprint(self,arg)
	local ret, val = mfunc(Janosh, unpack(arg))
	if ret ~= 0 then
    error("Test failed:" .. debug.traceback());
  end

end

function JanoshClass.ntest(self, ...)
  local arg={...}
  mfunc = table.remove(arg,1);
--  JanoshClass.tprint(self,arg)
        local ret, val = mfunc(Janosh, unpack(arg))
        if ret == 0 then
    error("Test failed:" .. debug.traceback());
  end
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

function JanoshClass.forever()
	while true do
  	Janosh:sleep(100000)
	end
end

function JanoshClass.error()
	error(debug.traceback())
end

return JanoshClass:new()

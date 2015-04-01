#!/usr/local/bin/luajit

if __JanoshFirstStart then
lanes = require "lanes".configure{ on_state_create = janosh_install; verbose_errors = true; }
end
signal = require "posix.signal"
posix = require "posix"

require "zmq"
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

function JanoshClass.setenv(self,key,value)
	posix.setenv(key,value)
end

function JanoshClass.system(self, cmdstring) 
 os.execute(cmdstring)
end

function JanoshClass.term(self, pid) 
  self:kill(pid, signal.SIGTERM)
end

function JanoshClass.kill(self, pid)
  self:kill(pid, signal.SIGKILL)
end


function JanoshClass.kill(self, pid, sig)
 signal.kill(pid,sig)
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


function JanoshClass.tprint(self, tbl, indent)
  if not indent then indent = 0 end
  for k, v in pairs(tbl) do
    formatting = string.rep("  ", indent) .. k .. ": "
    if type(v) == "table" then
      print(formatting)
      tprint(v, indent+1)
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
        assert(ret ~= nil, "execp() failed")

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
  return t[1];
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
  t = lanes.gen("*", function() 
		janosh_register_thread("Subscriber")
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
  janosh_publish({key,op,value});
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
      status, msg = pcall(function() callback(key, op, value) end)
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

function JanoshClass.unlock(self, name)
  janosh_unlock(name)
end

function JanoshClass.exec(self, commands) 
  for i, cmd in ipairs(commands) do
	  os.execute(cmd);
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

return JanoshClass:new()

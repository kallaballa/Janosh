#!/usr/local/bin/janosh -f

function test_mkarray()
	Janosh:transaction(function() 
		Janosh:truncate()
 		Janosh:test(Janosh.mkarr, "/array/.")
		Janosh:ntest(Janosh.mkarr, "/array/.")
 		Janosh:test(Janosh.set,"/bla", "blu")
		Janosh:ntest(Janosh.mkarr,"/bla/.")
    Janosh:publish("count");
	end)
end

function test_mkobject()
	Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkobj, "/object/.")
		Janosh:ntest(Janosh.mkobj, "/object/.")
		Janosh:test(Janosh.set, "/bla", "blu")
		Janosh:ntest(Janosh.mkobj, "/bla/.")
    Janosh:publish("count");
	end)
end


function test_append()
  Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
  	Janosh:test(Janosh.append,"/array/.",{"0","1","2","3"})
  	if Janosh:size("/array/.")  ~= 4 then Janosh:error() end
  	Janosh:test(Janosh.mkobj, "/object/.")
  	Janosh:ntest(Janosh.append, "/object/.", "0", "1", "2", "3")
  	if Janosh:size("/object/.") ~= 0 then Janosh:error() end
    Janosh:publish("count");
	end)
end

function test_set()
  Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
  	Janosh:test(Janosh.append,"/array/.","0")
	  Janosh:test(Janosh.set,"/array/#0","1")
  	Janosh:ntest(Janosh.set,"/array/#6","0")
  	if Janosh:size("/array/.") ~= 1 then Janosh:error() end

	  Janosh:test(Janosh.mkobj,"/object/.")
  	Janosh:test(Janosh.set,"/object/bla", "1")
	 	if Janosh:size("/object/.") ~= 1 then Janosh:error()	end
    Janosh:publish("count");
	end)
end


function test_add()
	Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.add,"/array/#0","1")
		Janosh:ntest(Janosh.add,"/array/#0","0")
		Janosh:ntest(Janosh.add,"/array/#5","0")
		if Janosh:size("/array/.") ~= 1 then Janosh:error() end

		Janosh:test(Janosh.mkobj,"/object/.")
		Janosh:test(Janosh.add,"/object/0","1")
		Janosh:ntest(Janosh.add,"/object/0","0")
		Janosh:test(Janosh.add,"/object/5","0")
 		if Janosh:size("/object/.") ~= 2 then	Janosh:error()	end
    Janosh:publish("count");
	end)
end

function test_remove()
  Janosh:transaction(function()
		Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.add,"/array/#0","1")
		Janosh:ntest(Janosh.add,"/array/#0","0")
		Janosh:test(Janosh.append,"/array/.",{"2","3","4"})
		Janosh:test(Janosh.remove,"/array/#1")
		if Janosh:get("/array/#1") ~= "3" then	Janosh:error()	end

		Janosh:ntest(Janosh.add,"/array/#5","0")
		Janosh:test(Janosh.remove,"/array/*")
		Janosh:ntest(Janosh.mkarr,"/array/#1/.")
		if Janosh:size("/array/.") ~= 0 then Janosh:error() end

		Janosh:test(Janosh.mkobj,"/object/.")
		Janosh:test(Janosh.add,"/object/0","1")
		Janosh:ntest(Janosh.add,"/object/0","0")
		Janosh:test(Janosh.add,"/object/5","0")
		Janosh:test(Janosh.mkarr,"/object/subarr/.")
		Janosh:test(Janosh.remove,"/object/.")
		Janosh:test(Janosh.size,"/object/.")
    Janosh:publish("count");
	end)
end

function test_replace()
  Janosh:transaction(function()
		Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.append,"/array/.","0")
		Janosh:test(Janosh.replace,"/array/#0","2")
		Janosh:ntest(Janosh.replace,"/array/#6","0")
    if Janosh:size("/array/.") ~= 1 then Janosh:error() end
 
		Janosh:test(Janosh.mkobj,"/object/.")
		Janosh:test(Janosh.set,"/object/bla","1")
		Janosh:test(Janosh.replace,"/object/bla","2")
		Janosh:ntest(Janosh.replace,"/object/blu","0")
    if Janosh:size("/object/.") ~= 1 then Janosh:error() end
    Janosh:publish("count");
	end)
end

function test_copy()
  Janosh:transaction(function()
		Janosh:truncate()
		Janosh:test(Janosh.mkobj,"/object/.")
		Janosh:test(Janosh.mkobj,"/target/.")
		Janosh:test(Janosh.mkarr,"/object/array/.")
		Janosh:test(Janosh.append,"/object/array/.",{"0","1","2","3"})
		Janosh:test(Janosh.copy,"/object/array/.","/target/array/.")
		Janosh:ntest(Janosh.copy,"/object/array/.","/target/.")
		Janosh:ntest(Janosh.copy,"/object/.","/object/array/.")
		Janosh:ntest(Janosh.copy,"/target/.","/object/array/.")
		if Janosh:size("/target/array/.") ~= 4 then Janosh:error() end
    if Janosh:size("/object/array/.") ~= 4 then Janosh:error() end
		if Janosh:get("/target/array/#0") ~= "0" then Janosh:error() end
    if Janosh:get("/object/array/#0") ~= "0" then Janosh:error()  end
    Janosh:publish("count");
	end)
end


function test_shift()
  Janosh:transaction(function()
		Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.append,"/array/.", { "0","1","2","3" })
		Janosh:test(Janosh.shift,"/array/#0","/array/#3")
    if Janosh:size("/array/.") ~= 4 then Janosh:error() end
    if Janosh:get("/array/#0") ~= "1" then Janosh:error() end
		if Janosh:get("/array/#3") ~= "0" then Janosh:error() end

		Janosh:test(Janosh.mkarr,"/array/#4/.")
		Janosh:test(Janosh.append,"/array/#4/.",{"3","2","1","0"})
		Janosh:test(Janosh.shift,"/array/#4/#3","/array/#4/#0")
		if Janosh:get("/array/#4/#0") ~= "0" then Janosh:error() end
    if Janosh:get("/array/#4/#3") ~= "1" then Janosh:error() end

		Janosh:test(Janosh.append,"/array/.",{"4","5","6","7"})
		Janosh:test(Janosh.mkarr,"/array/#9/.")
		Janosh:test(Janosh.append,"/array/#9/.",{"7","6","5","4"})
		Janosh:test(Janosh.shift,"/array/#4/.","/array/#9/.")
    if Janosh:size("/array/#8/.") ~= 4 then Janosh:error() end
    if Janosh:size("/array/#9/.") ~= 4 then Janosh:error() end
    if Janosh:get("/array/#8/#0") ~= "7" then Janosh:error() end
		Janosh:publish("count");
	end)
end

function test_shift_dir()
		Janosh:truncate() 
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.mkobj,"/array/#0/.")
		Janosh:test(Janosh.set,"/array/#0/label","0")
		Janosh:test(Janosh.set,"/array/#0/balast","0")

		Janosh:test(Janosh.mkobj,"/array/#1/.")
 		Janosh:test(Janosh.set,"/array/#1/label","1")
		Janosh:test(Janosh.set,"/array/#1/balast","1")

		Janosh:test(Janosh.mkobj,"/array/#2/.")
		Janosh:test(Janosh.set,"/array/#2/label","2")
		Janosh:test(Janosh.set,"/array/#2/balast","2")

		Janosh:test(Janosh.mkobj,"/array/#3/.")
		Janosh:test(Janosh.set,"/array/#3/label","3")
		Janosh:test(Janosh.set,"/array/#3/balast","3")

		Janosh:test(Janosh.shift,"/array/#2/.","/array/#0/.")
--  [ `test(Janosh.-r get /array/#0/label` -eq 2  ] || return 1
--  [ `test(Janosh.-r get /array/#1/label` -eq 0  ] || return 1
--  [ `test(Janosh.-r get /array/#2/label` -eq 1  ] || return 1

		Janosh:test(Janosh.shift,"/array/#1/.","/array/#0/.")
--  [ `test(Janosh.-r get /array/#0/label` -eq 0  ] || return 1
--  [ `test(Janosh.-r get /array/#1/label` -eq 2  ] || return 1
--  [ `test(Janosh.-r get /array/#2/label` -eq 1  ] || return 1

		Janosh:test(Janosh.shift,"/array/#1/.","/array/#2/.")
--  [ `test(Janosh.-r get /array/#0/label` -eq 0  ] || return 1
--  [ `test(Janosh.-r get /array/#1/label` -eq 1  ] || return 1
--  [ `test(Janosh.-r get /array/#2/label` -eq 2  ] || return 1

		Janosh:test(Janosh.shift,"/array/#0/.","/array/#3/.")
--  [ `test(Janosh.-r get /array/#0/label` -eq 1  ] || return 1
--  [ `test(Janosh.-r get /array/#1/label` -eq 2  ] || return 1
--  [ `test(Janosh.-r get /array/#2/label` -eq 3  ] || return 1
--  [ `test(Janosh.-r get /array/#3/label` -eq 0  ] || return 1
end

if not _MT_ then
	test_mkarray()
	test_mkobject()
	test_append()
	test_set()
	test_add()
	test_remove()
	test_replace()
	test_copy()
	test_shift()
	test_shift_dir()

	print("SUCCESS")
else
	local FIRST=Janosh:epoch()
	local CNT=0

	function count(key)
		Janosh:transaction(function()
			now=Janosh:epoch()
			diff=now - FIRST + 1

			CNT=CNT+1
			print(CNT/diff)
			if CNT % 100 == 0	then
				CNT=CNT/diff
				FIRST=Janosh:epoch()
			end
		end)
	end 

	function publishInThread(key) 
		Janosh:thread(function()
	  	while true do
	    	Janosh:publish(key)
		  end
		end)()
	end
	Janosh:subscribe("test_mkarray", test_mkarray)
	Janosh:subscribe("test_mkobject", test_mkobject)
	Janosh:subscribe("test_append", test_append)
	Janosh:subscribe("test_set", test_set)
	Janosh:subscribe("test_add", test_add)
	Janosh:subscribe("test_remove",test_remove)
	Janosh:subscribe("test_replace",test_replace)
	Janosh:subscribe("test_copy",test_copy)
	Janosh:subscribe("test_shift",test_shift)
	Janosh:subscribe("count",count)
	publishInThread("test_mkarray", test_mkarray)
	publishInThread("test_mkobject", test_mkobject)
	publishInThread("test_append", test_append)
	publishInThread("test_set", test_set)
	publishInThread("test_add",test_add)
	publishInThread("test_remove",test_remove)
	publishInThread("test_replace",test_replace)
	publishInThread("test_copy",test_copy)

	while true do
		Janosh:publish("test_shift");
	end
end

#!/usr/local/bin/janosh -f

function test_mkarray()
	Janosh:transaction(function() 
		Janosh:truncate()
 		Janosh:test(Janosh.mkarr, "/array/.")
		Janosh:ntest(Janosh.mkarr, "/array/.")
 		Janosh:test(Janosh.set,"/bla", "blu")
		Janosh:ntest(Janosh.mkarr,"/bla/.")
	end)
end

function test_mkobject()
	Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkobj, "/object/.")
		Janosh:ntest(Janosh.mkobj, "/object/.")
		Janosh:test(Janosh.set, "/bla", "blu")
		Janosh:ntest(Janosh.mkobj, "/bla/.")
	end)
end


function test_append()
  Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
  	Janosh:test(Janosh.append,"/array/.",{"0","1","2","3"})
  	if Janosh:size("/array/.")  ~= 4 then
			error()
		end
  	Janosh:test(Janosh.mkobj, "/object/.")
  	Janosh:ntest(Janosh.append, "/object/.", "0", "1", "2", "3")
  	if Janosh:size("/object/.") ~= 0 then
    	error()
  	end
	end)
end

function test_set()
  Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
  	Janosh:test(Janosh.append,"/array/.","0")
	  Janosh:test(Janosh.set,"/array/#0","1")
  	Janosh:ntest(Janosh.set,"/array/#6","0")
  	if Janosh:size("/array/.") ~= 1 then
    	error()
  	end

	  Janosh:test(Janosh.mkobj,"/object/.")
  	Janosh:test(Janosh.set,"/object/bla", "1")
  	if Janosh:size("/object/.") ~= 1 then
    	error()
  	end
	end)
end


function test_add()
	Janosh:transaction(function()
    Janosh:truncate()
		Janosh:test(Janosh.mkarr,"/array/.")
		Janosh:test(Janosh.add,"/array/#0","1")
		Janosh:ntest(Janosh.add,"/array/#0","0")
		Janosh:ntest(Janosh.add,"/array/#5","0")

		
		if Janosh:size("/array/.") ~= 1 then
			error()
		end

		Janosh:test(Janosh.mkobj,"/object/.")
		Janosh:test(Janosh.add,"/object/0","1")
		Janosh:ntest(Janosh.add,"/object/0","0")
		Janosh:test(Janosh.add,"/object/5","0")
 		if Janosh:size("/object/.") ~= 2 then
    	error()
 		end
	end)
end

function test_remove()
	Janosh:truncate()
	Janosh:test(Janosh.mkarr,"/array/.")
	Janosh:test(Janosh.add,"/array/#0","1")
	Janosh:ntest(Janosh.add,"/array/#0","0")
	Janosh:test(Janosh.append,"/array/.",{"2","3","4"})
	Janosh:test(Janosh.remove,"/array/#1")
  if Janosh:get("/array/#1")[1] ~= "3" then
    error()
  end

	Janosh:ntest(Janosh.add,"/array/#5","0")
	Janosh:test(Janosh.remove,"/array/*")
	Janosh:ntest(Janosh.mkarr,"/array/#1/.")
  if Janosh:size("/array/.") ~= 0 then
    error()
  end

	Janosh:test(Janosh.mkobj,"/object/.")
	Janosh:test(Janosh.add,"/object/0","1")
	Janosh:ntest(Janosh.add,"/object/0","0")
	Janosh:test(Janosh.add,"/object/5","0")
	Janosh:test(Janosh.mkarr,"/object/subarr/.")
	Janosh:test(Janosh.remove,"/object/.")
	Janosh:test(Janosh.size,"/object/.")
end
--[===[

function test_replace() {
 Janosh:test(Janosh.mkarr /array/.             || return 1
 Janosh:test(Janosh.append /array/. 0 		      || return 1
 Janosh:test(Janosh.replace /array/#0 2 		    || return 1
 Janosh:test(Janosh.replace /array/#6 0		      && return 1
  [ `test(Janosh.size /array/.` -eq 1 ] 	|| return 1
 Janosh:test(Janosh.mkobj /object/. 	 	        || return 1
 Janosh:test(Janosh.set /object/bla 1		      || return 1
 Janosh:test(Janosh.replace /object/bla 2		  || return 1
 Janosh:test(Janosh.replace /object/blu 0		  && return 1
  [ `test(Janosh.size /object/.` -eq 1 ] || return 1
}


function test_copy() {
 Janosh:test(Janosh.mkobj /object/.                      || return 1
 Janosh:test(Janosh.mkobj /target/.                      || return 1
 Janosh:test(Janosh.mkarr /object/array/.                || return 1
 Janosh:test(Janosh.append /object/array/. 0 1 2 3       || return 1
 Janosh:test(Janosh.copy /object/array/. /target/array/. || return 1
 Janosh:test(Janosh.copy /object/array/. /target/.       && return 1  
 Janosh:test(Janosh.copy /object/. /object/array/.       && return 1
 Janosh:test(Janosh.copy /target/. /object/array/.       && return 1
  [ `test(Janosh.size /target/array/.` -eq 4 ] 	  || return 1
  [ `test(Janosh.size /object/array/.` -eq 4 ]     || return 1
  [ `test(Janosh.-r get /target/array/#0` -eq 0 ]  || return 1
  [ `test(Janosh.-r get /object/array/#0` -eq 0 ]  || return 1
}

function test_shift() {
 Janosh:test(Janosh.mkarr /array/.                 || return 1
 Janosh:test(Janosh.append /array/. 0 1 2 3        || return 1
 Janosh:test(Janosh.shift /array/#0 /array/#3        || return 1
  [ `test(Janosh.size /array/.` -eq 4 ]      || return 1
  [ `test(Janosh.-r get /array/#0` -eq 1 ]    || return 1
  [ `test(Janosh.-r get /array/#3` -eq 0 ]    || return 1
 Janosh:test(Janosh.mkarr /array/#4/.               || return 1
 Janosh:test(Janosh.append /array/#4/. 3 2 1 0      || return 1
 Janosh:test(Janosh.shift /array/#4/#3 /array/#4/#0    || return 1
  [ `test(Janosh.-r get /array/#4/#0` -eq 0  ] || return 1  
  [ `test(Janosh.-r get /array/#4/#3` -eq 1  ] || return 1
 Janosh:test(Janosh.append /array/. 4 5 6 7        || return 1
 Janosh:test(Janosh.mkarr /array/#9/.               || return 1
 Janosh:test(Janosh.append /array/#9/. 7 6 5 4      || return 1
 Janosh:test(Janosh.shift /array/#4/. /array/#9/.    || return 1
  [ `test(Janosh.size /array/#8/.` -eq 4 ]    || return 1
  [ `test(Janosh.size /array/#9/.` -eq 4 ]    || return 1
  [ `test(Janosh.-r get /array/#8/#0` -eq 7  ] || return 1
}

function test_shift_dir() {
 Janosh:test(Janosh.mkarr /array/.                 || return 1
 Janosh:test(Janosh.mkobj /array/#0/.              || return 1
 Janosh:test(Janosh.set /array/#0/label 0          || return 1
 Janosh:test(Janosh.set /array/#0/balast 0          || return 1

 Janosh:test(Janosh.mkobj /array/#1/.              || return 1
 Janosh:test(Janosh.set /array/#1/label 1          || return 1
 Janosh:test(Janosh.set /array/#1/balast 1          || return 1

 Janosh:test(Janosh.mkobj /array/#2/.              || return 1
 Janosh:test(Janosh.set /array/#2/label 2          || return 1
 Janosh:test(Janosh.set /array/#2/balast 2          || return 1

 Janosh:test(Janosh.mkobj /array/#3/.              || return 1
 Janosh:test(Janosh.set /array/#3/label 3          || return 1
 Janosh:test(Janosh.set /array/#3/balast 3          || return 

 Janosh:test(Janosh.shift /array/#2/. /array/#0/.  || return 1
  [ `test(Janosh.-r get /array/#0/label` -eq 2  ] || return 1
  [ `test(Janosh.-r get /array/#1/label` -eq 0  ] || return 1
  [ `test(Janosh.-r get /array/#2/label` -eq 1  ] || return 1

 Janosh:test(Janosh.shift /array/#1/. /array/#0/.  || return 1
  [ `test(Janosh.-r get /array/#0/label` -eq 0  ] || return 1
  [ `test(Janosh.-r get /array/#1/label` -eq 2  ] || return 1
  [ `test(Janosh.-r get /array/#2/label` -eq 1  ] || return 1

 Janosh:test(Janosh.shift /array/#1/. /array/#2/.  || return 1
  [ `test(Janosh.-r get /array/#0/label` -eq 0  ] || return 1
  [ `test(Janosh.-r get /array/#1/label` -eq 1  ] || return 1
  [ `test(Janosh.-r get /array/#2/label` -eq 2  ] || return 1

 Janosh:test(Janosh.shift /array/#0/. /array/#3/.  || return 1
  [ `test(Janosh.-r get /array/#0/label` -eq 1  ] || return 1
  [ `test(Janosh.-r get /array/#1/label` -eq 2  ] || return 1
  [ `test(Janosh.-r get /array/#2/label` -eq 3  ] || return 1
  [ `test(Janosh.-r get /array/#3/label` -eq 0  ] || return 1
}
--]===]

test_mkarray()
test_append()
test_set()
test_add()
test_remove()
print("SUCCESS")
--[===[

function publishInThread(key) 
	Janosh:thread(function()
  	while true do
    	Janosh:publish(key)
	  end
	end)()
end
Janosh:subscribe("test_mkarray", test_mkarray);
Janosh:subscribe("test_mkobject", test_mkobject);
Janosh:subscribe("test_append", test_append);
Janosh:subscribe("test_set", test_set);
Janosh:subscribe("test_add", test_add);

publishInThread("test_mkarray", test_mkarray);
publishInThread("test_mkobject", test_mkobject);
publishInThread("test_append", test_append);
publishInThread("test_set", test_set);


while true do
	Janosh:publish("test_add");
end
--]===]


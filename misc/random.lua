#!/usr/local/bin/janosh -f

while true do
	local start = Janosh:epoch()
	Janosh:transaction(function()
	        for i=1,100 do
			local randUsername = Janosh:random("/lb/genderindex/male/.").genderindex.male[1]
		        local randUser = Janosh:getJson("/lb/users/" .. randUsername .. "/.")	
		end
	end)
	print(Janosh:epoch() - start)
end

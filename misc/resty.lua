local template = require "resty.template"
-- Using template.new
local view = template.new "view.html"
view.message = "Hello, World!"
view:render()
-- Using template.render
template.render("view.html", { message = "Hello, World!" })

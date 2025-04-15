--dofile("../tests/test-ALL.lua")
local files = {
    "test-classdef.lua",
    "test-class-object-fields.lua",
    "test-polymorphism.lua",
    "test-private-constructor.lua",
    "test-public-private.lua",
    "test-static-field.lua",
    "test-super-static-field.lua",
    "test-SingleInstance.lua",
    "test-super-method-call.lua",
    "test-leaktest.lua",
    "test-metamethod.lua",
    "test-class-polymorphism.lua",
    "test-field-redef.lua",
    "test-const-method.lua",
    "test-ret.lua",
    "test-typeof.lua",
    "test-instanceof.lua",
    "test-lib.lua",
    "test-advegdemo.lua",
    "test-object-field.lua",
    "test-constructor_selfcall.lua",
    "test-abstract.lua",
    "test-lambda.lua",
    "test-dyn-field.lua",
    "test-hotfix.lua",
    "test-classonoff.lua",
}
for i, file in ipairs(files) do
    local path = "../tests/" .. file
    print("Running on " .. path)
    dofile(path)
    collectgarbage()
    print("Done for " .. path)
end
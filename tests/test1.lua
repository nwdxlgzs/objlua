@class_on
-- dofile("../tests/test-ALL.lua")
@class_off
print(class)
@class_on
class A{}
print(load("print(class)"))
@class_off
print(class,A)


local class A{}
local class B:A{}
local class C{}
local b= B()
print(b instanceof A, b instanceof B, b instanceof C)
print(b instanceof A(), b instanceof B(), b instanceof C())

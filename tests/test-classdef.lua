class A{}
print(A)
local class B{}
print(B)
class C:A{}
print(C)
local class D:C{}
print(D)

class A{
    A(a:number){print(a)}}
class B:A{}
b=B(1)
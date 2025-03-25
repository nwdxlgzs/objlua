local a="abc"
print(a typeof "number")
if(a typeof "number") then
    print("CHECK_1 FAILED")
    error()
else
    print("CHECK_1 PASSED")
end

a=10
print(a typeof "number")
if(a typeof "number") then
    print("CHECK_2 PASSED")
else
    print("CHECK_2 FAILED")
    error()
end
class A{}
class B{}
a=A()
print(a typeof A)
print(a typeof B)
print(a typeof A())
print(a typeof B())
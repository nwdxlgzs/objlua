class A{
    static a=1234;
}
print(A)
print(A.a)-- 1234
A.a=5678
print(A.a)-- 5678
xpcall(function()
    print(A.b)-- field/method 'b' not found
end,print)
local class B{
    public const static b="wasd";
}
print(B.b)-- wasd
xpcall(function()
    B.b="qwer"
end,print)
print(B.b)-- wasd
local class C{
    public const static abc;
}
print(C.abc)-- nil
C.abc="firstSet"
print(C.abc)-- firstSet
xpcall(function()
    C.abc="secondSet"--const field 'abc' cannot be modified
end,print)
print(C.abc)-- firstSet
class A{
    public static KKK=321;
}
class B:A{
    public static LLL=234;
}
b=B()
print(b)
print(b.LLL)
print(b.KKK)
A.KKK=123
print(b.KKK)
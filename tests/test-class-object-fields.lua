local class FieldClass{
    static a;
    public b;
}
local a=FieldClass()
a.b=1
a.a=3
print(a.a)
print(a.b)
local b=FieldClass()
b.b=2
b.a=4
print(b.a, b.b)
print(a.a)
print(FieldClass.a)
print(a.b)
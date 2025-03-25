class A{
    b=123;
    B={self.b,2,3};
}
a=A()
for k,v in pairs(a.B)do
    print(k,v)
end
a.B[1]=234
for k,v in pairs(a.B)do
    print(k,v)
end
a=A()
for k,v in pairs(a.B)do
    print(k,v)
end

class A{
    @nowrap a={};
}
A().a[1]="nowrap"
print(A().a[1])



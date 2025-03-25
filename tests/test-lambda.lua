class A{
    static a(b:function) -> 1,2,3,b();
}
print(A.a(function()return 4,5,6;end))
class A{
    static a = 1;
}
print(A.a);
xpcall(function()
    class B:A{
        static a = 2;
    }
    print(B.a,A.a);
end,print);

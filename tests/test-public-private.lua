local class A{
    private A(a:string){
        print(a)
    };
    static test(){
        print(self,self.Value)
        A("E")
    };
    private static AAA(){
        print(self.Value+1)
    };
    private static Value = 1;
}
print(A)
A.test()
xpcall(function()
    A("e")
end,print)
xpcall(function()
    print(A.Value)
end,print)
xpcall(function()
    print(A.AAA())
end,print)
xpcall(function()
A.Value = 2
end,print)
A.test()

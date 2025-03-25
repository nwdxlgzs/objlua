local class A{
    private A(a:string){
        print(a)
    };
    static test(){
        print("Hello")
        A("E")
    }
}
A.test()
xpcall(function()
    A("e")
end,print)
class A{
    static const CallMe(a:any){
        print("Maybe");
    }
}
print(A.CallMe(6));
xpcall(function()
    class B:A {
        static CallMe(a:any){
            print("Proxy");
        }
    }
    print(B.CallMe(6));
end,print);

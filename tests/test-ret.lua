class A{
    static const CallMe(a:number,...){
        print(a,"Maybe",...);
        return true,a,...;
    }
}
print(A.CallMe(1,2,3,4));

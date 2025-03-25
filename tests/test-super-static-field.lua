local class A{
    static a=1;
}
local class B:A{
    public B(){
        print(super.a,self.a)
        super.a=2
        print(super.a,self.a)
        self.a=3
        print(super.a,self.a)
    }
}
B()

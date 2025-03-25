class A{
    A(a:number, b:number){
        print(a, b)
    }
    A(a:number){
        print(self)
        self.A(a, 0)
    }
}
A(1)
A(1, 2)
class A{}
class eA:A{}
class B{
    B(a:<A>){print("a:A",a)};
    B(a:string){print("a:string",a)};
}
B(A())
B(eA())
B("str")


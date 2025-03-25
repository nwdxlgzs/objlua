class Base{
    Base(...){}
}
class A:Base{
    A(a:string){
       print("string--",a,self,super)
    }
    A(a:number){
       print("number--",a,self,super)
    }
    A(a){
       print("no-polymorphism<overload>--",a,self,super)
    }
}
print(A(123),A("abc"),A(true))
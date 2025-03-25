class A{
    private const @nowrap a=11;
    b(c:number){}
}
m=objlua.getDeclaredMethods(A)[1]
objlua.hotfixMethod(m,function(self,super,c)
    self,super=objlua.getMethodInit()
    print(self.a+c)
    end)
A().b(2)
f=objlua.getDeclaredFields(A)[1]
print(objlua.getFieldValue(f))
objlua.setFieldValue(f,100)
A().b(2)

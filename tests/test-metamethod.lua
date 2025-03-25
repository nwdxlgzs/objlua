class MetaTest{
    @meta __add(a:number){
        print(a)
        return 666
    };
    @meta __add(a:string){
        return "MetaTest:"..tostring(self)
    };
    @meta __tostring(){
        return "Redefine[__tostring]"
    }
}
local o = MetaTest()
print(o+1)
print(o+"a")

do
    local class A{@meta __gc(){print(1)}} do a=A() end
end

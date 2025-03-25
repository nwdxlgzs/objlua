
function dump(o)
    local t = {}
    local _t = {}
    local _n = {}
    local space, deep = string.rep(' ', 2), 0
    local function _ToString(o, _k)
        if type(o) == ('number') then
            table.insert(t, o)
        elseif type(o) == ('string') then
            table.insert(t, string.format('%q', o))
        elseif type(o) == ('table') then
            local mt = getmetatable(o)
            if mt and mt.__tostring then
                table.insert(t, tostring(o))
            else
                deep = deep + 2
                table.insert(t, '{')

                for k, v in pairs(o) do
                    if v == _G then
                        table.insert(t, string.format('\r\n%s%s\t=%s ;', string.rep(space, deep - 1), k, "_G"))
                    elseif v ~= package.loaded then
                        if tonumber(k) then
                            k = string.format('[%s]', k)
                        else
                            k = string.format('[\"%s\"]', k)
                        end
                        table.insert(t, string.format('\r\n%s%s\t= ', string.rep(space, deep - 1), k))
                        if v == NIL then
                            table.insert(t, string.format('%s ;',"nil"))
                        elseif type(v) == ('table') then
                            if _t[tostring(v)] == nil then
                                _t[tostring(v)] = v
                                local _k = _k .. k
                                _t[tostring(v)] = _k
                                _ToString(v, _k)
                            else
                                table.insert(t, tostring(_t[tostring(v)]))
                                table.insert(t, ';')
                            end
                        else
                            _ToString(v, _k)
                        end
                    end
                end
                table.insert(t, string.format('\r\n%s}', string.rep(space, deep - 1)))
                deep = deep - 2
            end
        else
            table.insert(t, tostring(o))
        end
        table.insert(t, " ;")
        return t
    end

    t = _ToString(o, '')
    return table.concat(t)
end
class A{
    public A(){}
    public A(a){}
    public test(){}
    public test(a:any){}
    public static test2(){}
    public static test2(a){}
    public test3;
    public static test4;
}
class B:A{
    public test(){print("testCallMe")}
    public static test2(){}
}
for k, v in pairs(objlua) do
    print(k, v)
end
--相似的库函数就不写了，示范典型。库函数不包括hotfix相关，这个单独测试
print(objlua.getSuper(B),objlua.getSuper(B()))
print(objlua.getClass(B),objlua.getClass(B()))
print(objlua.isClass(B),objlua.isClass(B()))
print(objlua.isObject(B),objlua.isObject(B()))
print(dump(objlua.getConstructors(B)), dump(objlua.getDeclaredConstructors(B)))
print(dump(objlua.getMethods(B)), dump(objlua.getDeclaredMethods(B)))
print(dump(objlua.getFields(B)), dump(objlua.getDeclaredFields(B)))
print(dump(objlua.getMetamethods(B)), dump(objlua.getDeclaredMetamethods(B)))
f=objlua.getFields(B)
print("public:",objlua.isPublic(f[1]),
      "private:",objlua.isPrivate(f[1]),
      "static:",objlua.isStatic(f[1]),
      "const:",objlua.isConst(f[1]),
      "meta:",objlua.isMeta(f[1]),
      "abstract:",objlua.isAbstract(f[1]),
      "constructor:",objlua.isConstructor(f[1]),
      "nowrap:",objlua.isNoWrap(f[1]),
      "ismethod:",objlua.isMethod(f[1]),
      "isfield:",objlua.isField(f[1]))

print(objlua.getName(f[1]),objlua.getName(B))
objlua.getMethodFunction(objlua.getDeclaredMethods(B)[1])()
print(objlua["typeof"](B(),B),objlua["typeof"](B(),A))
print(objlua["instanceof"](B(),B),objlua["instanceof"](B(),A))

class B{}
class A{
    a(a:any,b:string,c:<B>,...){}
}
m=objlua.getDeclaredMethods(A)[1]
print(dump(objlua.getMethodArgTypes(m)))

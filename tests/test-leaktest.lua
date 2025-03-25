function testMem(print)
    collectgarbage("collect")
    local notsetup_mem, setup_mem, cleanup_mem = collectgarbage("count"), nil, nil
    do
        --[[
         注册__gc后cleanup_mem - notsetup_mem必然不是0，
         可以通过setmetatable({}, {__gc = function() end})验证
         即使原始的Lua代码，在处理__gc后就是会”泄露“（不一定是泄露，也可能是回事时统计信息错误）
         这导致诸如：
         clazz->constructors = (LuaObjMethod **) luaM_reallocvector(L, clazz->constructors, clazz->size_constructors,
                                                                           clazz->size_constructors + 1, LuaObjMethod);
         的代码没通过Userdata正常获取而是用更底层API被迫需要注册__gc（或者更改lgc.c代码每次扫描释放Userdata时看一眼是不是类/对象，是就额外执行释放）
         太痛苦了，这里又不是真leak了，但是这里得不到0也是真不舒服，这导致LuaObjUData内部全量使用常规方案才能保证是0
         于是增加了一些其实很多余的代码，但是为了能得到0，方便感知其他情况是否有泄露，只能这样非常难受了
         这里循环执行，第一次leak不是0很正常（因为除弱表外，只有执行类相关功能后才会初始化一些内容），后续重复执行就全是0就没问题
        ]]
        local class A{}--setmetatable({}, {__gc = function() end})
        local a;
        for i = 1, 100 do
            a = A()
        end
    end
    setup_mem = collectgarbage("count")
    collectgarbage("collect")
    cleanup_mem = collectgarbage("count")
    print("notsetup_mem:", notsetup_mem, "setup_mem:", setup_mem, "cleanup_mem:", cleanup_mem)
    print("grow memory size:", setup_mem - notsetup_mem)
    print("leak:", cleanup_mem - notsetup_mem)
end
testMem(function()end)--第一次有初始化的事，肯定不是0，后续重复执行如果泄露肯定不是0这才需要看
for i = 1, 4 do
    testMem(print)
end

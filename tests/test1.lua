dofile("../tests/test-ALL.lua")
-- for i=1,4 do
--     collectgarbage("collect")
--     local notsetup_mem, setup_mem, cleanup_mem = collectgarbage("count"), nil, nil
--     do
--         local class C{
--             a(){}
--             c=1;
--             b(){
--                 self.a()
--             }
--         }
--         local c = C()
--         c.b()
--     end
--     setup_mem = collectgarbage("count")
--     collectgarbage("collect")
--     cleanup_mem = collectgarbage("count")
--     print("notsetup_mem:", notsetup_mem, "setup_mem:", setup_mem, "cleanup_mem:", cleanup_mem, "grow memory size:", setup_mem - notsetup_mem)
--     print("leak:", cleanup_mem - notsetup_mem)
-- end

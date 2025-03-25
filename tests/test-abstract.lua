class A{
    @abstract private static test(a:number);
}

class B:A{
    private static test(a:number){}
}
xpcall(function()
    local class A{
        @abstract private static test(a:number);
    }
    local class B:A{
        static test(a:any){}
    }
end,print)

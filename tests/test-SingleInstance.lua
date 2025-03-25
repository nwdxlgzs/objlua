class SingleInstance{
    private static instance;
    private SingleInstance(){};
    public static getInstance(){
        if(not self.instance) then
            self.instance = SingleInstance();
        end
        return self.instance;
    }
}
SI = SingleInstance.getInstance();
print(SI);
print(instance)
SI = SingleInstance.getInstance();
print(SI);
xpcall(function()
    SI = SingleInstance();
    print(SI);
end, print);
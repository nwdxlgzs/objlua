class Animal{
    public static count = 0;
    private name;
    public bark;
    public Animal(name:string){
        self.name = name
        Animal.count = Animal.count + 1
        self.bark = ""
    }
    getName(){
        return self.name
    }
    setName(name){
        self.name = name
    }
}

local class Cat:Animal{
    private age;
    private const dna;
    public Cat(name:string){
        self.age = 0
        super.bark = "ίχ"
        self.dna = math.random()
    }
    getName(){
        return "Cat<"..super.getName()..">"
    }
    setAge(age){
        self.age = age
    }
    getAge(){
        return self.age
    }
}
cat1 = Cat("»¨»¨")
cat2 = Cat("ίδίδ")
print(cat1.getName())
cat1.setAge(2)
print(cat1.getAge())
print(Animal.count)
print(cat2.bark)

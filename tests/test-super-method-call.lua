class A{
    public static S(){
        print("A")
    }
    public static S2(){
        print("B")
    }
}
class B:A{
    public static S(){
        super.S()
        print("S2Call")
    }
}
B.S()
B.S2()

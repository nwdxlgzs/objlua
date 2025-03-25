# 面向对象Lua语法(初始稿)

## 类/对象的定义
- `类/对象`是单独的一种数据类型，尽管它和`表`很像，但是为了不被表的相关函数操作污染以及实现其他功能，`类/对象`的类型并非`LUA_TTABLE`(`值为5`)，而为了最小化对第三方库的影响，它作为`LUA_TUSERDATA`(`值为7`)类型被管理，然而虚拟机内部把这类数据会独立标记管理使用（曾包括GC）。
- `类`是一种特别的`对象`数据。


## 面向对象语法的介绍
- 面向对象语法更多参考了`Java`语法风格，其中访问修饰符`public`、`private`概念也被设计使用：
  - **public**:不限制访问，默认值
  - **private**:仅限`类/对象`内部访问
  - `类`的定义作为一种Lua直接语法上管理的内容，它没有访问修饰符，定义时默认全局可见（在`_ENV`中），也可以使用`local`局部可见（但是不支持ToBeClose）。访问修饰符只约束字段和方法。
  - 访问修饰符底层基于运行的栈实现判断与校验
  - 构建器在对象内部会退化为一般方法，并且强制`private`
- `类`定义以`class`为关键字完成定义，定义范围限定在后面的花括号范围内。
- 为了防止出现“钻石问题”，面向对象语法不支持多类继承，要继承的类在`类名定义`+`:`后写出。
- 不需要实例化就可以使用的字段/方法需要加上关键字`static`。
- 方法支持`overload`，排查顺序为定义时顺序，多态的类型支持`luaT_typenames_`中的定义（~~`no value`/~~`nil`/`boolean`/`userdata`/`number`/`string`/`table`/`function`/`thread`/`upvalue`/`proto`），也支持类：
  - **type**类型：形参部分形如`pram:string`，特别的，`pram:any`不限制（只写变量时的默认情况）
  - **类**类型：形参部分形如`pram:<a>`（前提是定义时能访问到类`a`才能被运行，编译时无法判断）
  - 暂时和Lua的默认类型做分隔，如`pram:string`和`pram:<string>`不同，我们不建议定义和Lua默认类型一样的类。
  - 你不应该对`VARARG`(即`···`)要求任何类型（无参数或者仅`...`都相当于不设置类型，不在首轮查找中）
  - 多态查找详细顺序是优先在有类型定义的方法中寻找，没找到就返回NULL或者第一个没有设置类型的匹配项（程序不检查匹配`Proto`的任何内容）。
  - 构造函数不会在父类中查找，但是其他方法在未找到的情况下会向父类/对象进行查找。
  - 所有形参都未要求类型时和形参任一要求`:any`并不一样，前者视为放弃多态，后者参与多态并且支持匹配时根据参数数量是否匹配中。
- 定义成类名的方法为构建函数，即使不定义任何构建函数，仍然可以使用形如`a=A()`的形式获取到实例对象，但是这种情况当且仅当参数为0且无构建器定义时发生
- 类的字段/方法使用`;`完成分隔（字段强制要求，方法不强制）。
- 方法支持多态以及很多传统Lua特性的缘故，定义时放弃了Lua的`function-end`风格，使用花括号定义方法体以进行显著区分（执行规则仍然是函数），并且让类定义风格更偏向传统OOP惯用的风格
- 对象获取自己使用关键字`self`(变量)完成
- 方法/字段不得与关键词冲突（除非使用字符串方式定义）
- 调用继承类使用`super.xxx(yy,zz)`形式完成（存储了父对象实例，如果没有那么是`nil`），构造函数是特例，它会创建对象时自动调用。
- 允许使用关键字`const`拒绝复写（访问意义上的复写或者子类定义上的复写）。
- 字段允许定义时直接赋值，但是动态字段只鼓励在构建函数内完成，否则可能会产生大量零碎的函数辅助构建。
- 构建器本质也是方法
- 构建方法无论是否有`static`都允许调用（即无视`static`，设置的`static`也不会反应在标志位中）
- 支持对部分元方法的设置，对元方法的设置无视访问修饰符等（相当于强制`public static`，强制在标志位中体现），元方法也支持多态
- 对象的`static`和类的`static`执行规则并不一样，`self`/`super`指向的是对应的对象或类，被修饰`static`的对象方法操作不是操作的类的方法字段，他们有权力对对象非`static`方法字段进行访问操作（为了同步单一元方法可以对类/对象分别响应这一特性）。
- 方法做多只支持255-参数
- 子类会在创建时自动调用父类构建函数，没有构建函数的类都默认可以进行无参构建
- 子类方法不会执行时就调用父类同名方法，使用形如`super.m(p)`格式完成父方法调用
- 很显然，类有构建函数可以直接`__call`，但是对象内部构建器降级，就不能`__call`了
- 子类不能重写定义父类已经定义了的字段
- 查找顺序是字段->方法->构建器
- 使用`@abstract`注解的方法`{body}`方法体需要直接写成`;`，这会让继承的类必须完成定义同等的方法。
- 使用`@meta`注解的方法可以自动挂载为元方法，但是请注意，`__index`、`__newindex`、`__call`不支持设置，会直接报错警告，当然你可以定义类之后手动`getmetatable`再覆盖这三个，覆盖的前提是你知道你在做什么。
- 要求参数类型时可以写成`arg:nil`以及`arg:function`，不需要担心这两个是关键字而无法使用
- 因为`const`、`public`、`private`、`static`在定义方法与字段被认为是标志，是不能直接定义出如叫`const`等字段或者方法的，所以字段以及方法名提供直接通过字符串而非名字的方式定义，如`"const"`，同样的，也可以借助这个机制定义名叫`nil`的方法或者字段。
- 方法允许使用lambda表达式，在定义完参数后紧跟`->`，那么将直接使用返回值解析逻辑语法。
- 通过`@nowrap`对动态字段（`@nowrap`仅对动态字段且定义时就赋值时生效，其他情况会被忽略，反应在标志位中）注解，可以放弃构造闭包而直接使用定义字段时的值，如果不使用那么动态字段的值会转为闭包在创建时为每个对象单独初始化。
```lua
class Animal{
    public static count = 0;
    private name;
    public bark;
    public Animal(name:string){
        self.name = name
        Animal.count = Animal.count + 1
        self.bark = ""
    },
    getName(){
        return self.name
    },
    setName(name){
        self.name = name
    },
}
local class Cat:Animal{
    private age;
    private const dna;
    Cat(name){
        self.age = 0
        super.bark = "喵"
        self.dna = math.random()
    },
    getName(){
        return "Cat<"..super.getName()..">"
    },
    setAge(age){
        self.age = age
    },
    getAge(){
        return self.age
    },
}
cat1 = Cat("花花")
cat2 = Cat("咪咪")
```

# EBNF语法
```EBNF
	chunk ::= block

	block ::= {stat} [retstat]

	stat ::=  ‘;’ |
		 varlist ‘=’ explist | 
		 functioncall | 
		 label | 
		 break | 
		 goto Name | 
		 do block end | 
		 while exp do block end | 
		 repeat block until exp | 
		 if exp then block {elseif exp then block} [else block] end | 
		 for Name ‘=’ exp ‘,’ exp [‘,’ exp] do block end | 
		 for namelist in explist do block end | 
		 function funcname funcbody | 
		 local function Name funcbody | 
		 local attnamelist [‘=’ explist] |
		 [local] class Name [‘:’ Name] ‘{’ {classdefbody} ‘}’
	
	classdefbody ::= fielddef ‘;’ | methoddef [‘;’]
	
	annotate ::= ‘@’ Name
	
	fmflags ::= {annotate} | {Name}
	
	fielddef ::= {fmflags} Name [‘=’ exp]
	
	normalmethoddef ::= abstractmethoddef ‘{’ block ‘}’
	
	lambdamethoddef ::= abstractmethoddef ‘->’ [explist]
	
	abstractmethoddef ::= {fmflags} Name ‘(’ [methodparlist] ‘)’
	
	tmethoddef ::= normalmethoddef | lambdamethoddef | abstractmethoddef
	
	methodparlist ::= methodnamelist [‘,’ ‘...’] | ‘...’
	
	methodnamelist ::= typename {‘,’ typename}
	
	typename ::= Name [‘:’ Name] | Name [‘:’ ‘<’ Name ‘>’]

	attnamelist ::=  Name attrib {‘,’ Name attrib}

	attrib ::= [‘<’ Name ‘>’]

	retstat ::= return [explist] [‘;’]

	label ::= ‘::’ Name ‘::’

	funcname ::= Name {‘.’ Name} [‘:’ Name]

	varlist ::= var {‘,’ var}

	var ::=  Name | prefixexp ‘[’ exp ‘]’ | prefixexp ‘.’ Name 

	namelist ::= Name {‘,’ Name}

	explist ::= exp {‘,’ exp}

	exp ::=  nil | false | true | Numeral | LiteralString | ‘...’ | functiondef | 
		 prefixexp | tableconstructor | exp binop exp | unop exp 

	prefixexp ::= var | functioncall | ‘(’ exp ‘)’

	functioncall ::=  prefixexp args | prefixexp ‘:’ Name args 

	args ::=  ‘(’ [explist] ‘)’ | tableconstructor | LiteralString 

	functiondef ::= function funcbody

	funcbody ::= ‘(’ [parlist] ‘)’ block end

	parlist ::= namelist [‘,’ ‘...’] | ‘...’

	tableconstructor ::= ‘{’ [fieldlist] ‘}’

	fieldlist ::= field {fieldsep field} [fieldsep]

	field ::= ‘[’ exp ‘]’ ‘=’ exp | Name ‘=’ exp | exp

	fieldsep ::= ‘,’ | ‘;’

	binop ::=  ‘+’ | ‘-’ | ‘*’ | ‘/’ | ‘//’ | ‘^’ | ‘%’ | 
		 ‘&’ | ‘~’ | ‘|’ | ‘>>’ | ‘<<’ | ‘..’ | 
		 ‘<’ | ‘<=’ | ‘>’ | ‘>=’ | ‘==’ | ‘~=’ | 
		 and | or | typeof | instanceof

	unop ::= ‘-’ | not | ‘#’ | ‘~’
```
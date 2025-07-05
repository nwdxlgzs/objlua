# ObjLua - Lua面向对象
> **基于Lua5.4.7的面向对象实现**

<details>
  <summary>安全更新</summary>

> 虽然objlua是基于5.4.7版本开发，但是基于安全修复需求，现已支持5.4.8相关更新与截止2025年7月5日可以查到的对5.4.8的[修补补丁](https://lua.org/bugs.html)，即objlua等效基于5.4.8（严格的说是半个5.4.9）开发，这也是为什么你看到的版本号实际不是5.4.7的原因。

</details>



### [在线体验](https://nwdxlgzs.github.io/objlua/objlua-web.html)（Web端不定期更新，未必是最新版本）

# 面向对象语法

## 1. 类定义

- **类声明**：使用 `class` 关键字定义类，后接类名和花括号包裹的方法/字段
- **局部类定义**：`local class ClassName { ... }`
- **继承**：`class ChildClass: ParentClass { ... }`（仅支持单继承）
- **类构成**：由类名、方法、字段三要素组成

## 2. 构造方法

- **判定条件**：方法名与类名相同即为构造方法
- **特殊机制**：
  - 无视 `static` 标志
  - 自动调用父类构造方法（唯一会自动调用父方法的方法）
  - 对象内构造方法降级为方法可供使用（可以实现委托构造）
- **空构造处理**：无构造方法时允许任意参数实例化
- **返回值限制**：无论返回什么内容，实例化结果始终是类对象

## 3. 方法定义

```lua
[@abstract] [@meta] [public|private] [static] [const] 
methodName(params) { ... }
```
```lua
[@abstract] [@meta] [public|private] [static] [const] 
methodName(params) -> return_value
```
- **访问修饰符**:
  - `public`: 公开访问
  - `private`: 私有访问（类内访问）
- **限定符**:
  - `static`: 静态方法
  - `const`: 禁止子类覆盖
- **注解**:
  - `@abstract`: 抽象方法（需子类实现）
  - `@meta`: 元方法（自动设置到元表）
- **方法体**:
  - 方法体内有局部变量`self`和`super`可供使用，对应类或对象自己以及父类或者父对象
  - 直接返回返回值可以简写为：`->` + 返回值
- **参数匹配**：
  - 无类型约束时视为普通方法
  - 带类型约束时严格匹配参数类型
- **特殊方法名**：
  - 支持名称格式（`NAME`）和字符串格式（`"NAME"`）
  - 元方法禁止定义 `__index`, `__newindex`, `__call`


## 4. 字段定义

```lua
[@nowrap] [public|private] [static] [const] 
fieldName [= value]
```

- **访问修饰符**:
  - `public`: 公开访问
  - `private`: 私有访问（类内访问）
- **限定符**:
  - `static`: 静态字段
  - `const`: 禁止二次赋值
- **注解**:
  - `@nowrap`：动态字段跳过实例化的初始化
- **语法要求**:
  - 字段一定需要使用`;` 结尾
  - 支持名称格式（`NAME`）和字符串格式（`"NAME"`）
- **初始化**：
  - 从上到下依次定义
  - 动态字段建议避免直接定义赋值

## 5. 元方法
```lua
@meta public static __add(a, b) -> a.value + b.value;
```
- **判定条件**：使用 `@meta` 注解
- **强制修饰符**：自动启用`public` + `static`
- **元表限制**：
  - 禁止定义 `__index`, `__newindex`, `__call`
  - 支持元方法多态重载
- **执行环境**:
  - 静态方法中可用 `self` 访问类本身、可用 `super` 访问父类
  - 实例方法中可用 `self` 访问对象本身、可用 `super` 访问父对象



## 6. 抽象方法
```lua
@abstract public mustImplement();
```
- **判定条件**：使用 `@abstract` 注解
- **额外说明**：
  - 必须由直接子类实现该方法，不强制子类的子类实现
  - 语法上禁止定义方法体
  - 要求保持方法签名一致（除@abstract外）

## 7. 二元运算符

### typeof

`A typeof B`：

- **类/对象模式**：当B为类/对象时，检查A的类是否等于B的类
- **基础类型模式**：当B为普通类型时，等同于 `type(A) == B`

### instanceof

`A instanceof B`：

- **类模式**：当B是类时，检查A或其父类是否等于B
- **对象模式**：当B是对象时，检查A或其父对象是否等于B

## 8. 参数匹配
```lua
pram:type
```
```lua
pram:<type>
```
- **判定条件**：方法参数出现 `:`时
- **匹配模式**：
  - **基础类型模式**：形如`pram:type`时，要求类型结果必须等于字符串type
  - **类模式**：形如`pram:<type>`时，要求类型必须是类或对象，并且其或父类是type类
  - **不限制模式**：不限制类型，形如`pram:any`形式不限制类型，或者方法的参数匹配中至少出现过`基础类型模式`或`类模式`时可省略为`pram`
  - **不定长参数模式**：形如`...`且方法的参数匹配中出现至少一个`基础类型模式`或`类模式`或`不限制模式`时，可为不定长参数模式
  - **完全放弃参数匹配**：方法的参数中没出现任何`基础类型模式`或`类模式`或`不限制模式`时视为放弃参数匹配，当前类或对象在其他同名方法无法匹配时才被自动使用

## 9. 补充说明

- **新增关键字**：`class`, `typeof`, `instanceof`, `->`
- **兼容性设计**：访问修饰符/限定符不作为保留字
- **查找规则**：
  - 子类->父类
  - 字段->方法->构建器
- **性能建议**：
  - 避免在字段定义时直接对动态字段赋值，如果值是可直接拷贝的值，可以使用`@nowrap`注解跳过初始化
  - 动态字段最好在构造方法阶段初始化而非定义时

# 标准库(objlua)
| 库函数                   | 描述                                                                                                       |
|--------------------------|------------------------------------------------------------------------------------------------------------|
| getSuper                 | 获取父类或父对象，非类非对象返回 `nil`                                                                      |
| getClass                 | 获取类或对象的类，非类非对象返回 `nil`                                                                      |
| isClass                  | 判断当前输入是否为类，非类非对象返回 `false`                                                                |
| isObject                 | 判断当前输入是否为对象，非类非对象返回 `false`                                                              |
| getDeclaredFields        | 获取定义的字段                                                                                            |
| getFields                | 获取包含父类在内的全部字段                                                                                 |
| getDeclaredMethods       | 获取定义的方法（不包括元方法、构造方法、降级的构造方法、抽象方法）                                             |
| getMethods               | 获取包含父类在内的全部方法（不包括元方法、构造方法、降级的构造方法、抽象方法）                                  |
| getDeclaredConstructors  | 获取定义的构造方法                                                                                        |
| getConstructors          | 获取包含父类在内的全部构造方法                                                                             |
| getDeclaredMetamethods   | 获取定义的元方法                                                                                          |
| getMetamethods           | 获取包含父类在内的全部元方法                                                                               |
| getDeclaredAbstractMethods | 获取定义的抽象方法                                                                                      |
| getAbstractMethods       | 获取包含父类在内的全部抽象方法                                                                             |
| isPublic                 | 判断字段或方法是否有 `public` 访问修饰符                                                                   |
| isPrivate                | 判断字段或方法是否有 `private` 访问修饰符                                                                  |
| isStatic                 | 判断字段或方法是否有 `static` 限定符                                                                       |
| isConst                  | 判断字段或方法是否有 `const` 限定符                                                                        |
| isMeta                   | 判断字段或方法是否有 `@meta` 注解                                                                          |
| isAbstract               | 判断字段或方法是否有 `@abstract` 注解                                                                      |
| isConstructor            | 判断是否是构造方法                                                                                        |
| isNoWrap                 | 判断是否有 `@nowrap` 注解                                                                                 |
| isMethod                 | 判断是否是方法（需根据标志判断）                                                                            |
| isField                  | 判断是否是字段（需根据标志判断）                                                                            |
| getName                  | 获取类名、方法名、字段名，均无法获取时返回 `nil`                                                             |
| getMethodFunction        | 获取方法的实际 Lua 函数                                                                                    |
| typeof                   | `typeof` 二元运算的库函数版本                                                                              |
| instanceof               | `instanceof` 二元运算的库函数版本                                                                          |
| hotfixMethod             | 热修复方法，将方法替换为指定的新 Lua 函数（需显式声明 `self` 和 `super` 形参）                                |
| getMethodInit            | 手动执行 `OP_METHODINIT` 指令，返回 `self` 与 `super`                                                      |
| getFieldValue            | 获取字段值（动态字段未设置 `@nowrap` 时获取的是初始化函数）                                                  |
| setFieldValue            | 设置字段值且不触发 `const` 相关机制（动态字段可在有 `@nowrap` 标志的方法中设置初始化函数）                     |
| getMethodArgTypes        | 获取方法定义的参数声明，声明数量通过键 `nargs` 获知                                                          |
| hasMethod                | 指定类或对象、名称，判断是否存在对应方法                                                                     |
| hasField                 | 指定类或对象、名称，判断是否存在对应字段                                                                     |

# LuaAPI
```c
#define LUA_OPTYPEOF	3
#define LUA_OPINSTANCEOF	4
LUA_API int objlua_getSuper(lua_State *L);
LUA_API int objlua_getClass(lua_State *L);
LUA_API int objlua_isClass(lua_State *L);
LUA_API int objlua_isObject(lua_State *L);
LUA_API int objlua_getDeclaredFields(lua_State *L);
LUA_API int objlua_getFields(lua_State *L);
LUA_API int objlua_getDeclaredMethods(lua_State *L);
LUA_API int objlua_getMethods(lua_State *L);
LUA_API int objlua_getDeclaredConstructors(lua_State *L);
LUA_API int objlua_getConstructors(lua_State *L);
LUA_API int objlua_getDeclaredMetamethods(lua_State *L);
LUA_API int objlua_getMetamethods(lua_State *L);
LUA_API int objlua_getDeclaredAbstractMethods(lua_State *L);
LUA_API int objlua_getAbstractMethods(lua_State *L);
LUA_API int objlua_isPublic(lua_State *L);
LUA_API int objlua_isPrivate(lua_State *L);
LUA_API int objlua_isStatic(lua_State *L);
LUA_API int objlua_isConst(lua_State *L);
LUA_API int objlua_isMeta(lua_State *L);
LUA_API int objlua_isAbstract(lua_State *L);
LUA_API int objlua_isConstructor(lua_State *L);
LUA_API int objlua_isNoWrap(lua_State *L);
LUA_API int objlua_isMethod(lua_State *L);
LUA_API int objlua_isField(lua_State *L);
LUA_API int objlua_getName(lua_State *L);
LUA_API int objlua_getMethodFunction(lua_State *L);
LUA_API int objlua_typeof(lua_State *L);
LUA_API int objlua_instanceof(lua_State *L);
LUA_API int objlua_hotfixMethod(lua_State *L);
LUA_API int objlua_getMethodInit(lua_State *L);
LUA_API int objlua_getFieldValue(lua_State *L);
LUA_API int objlua_setFieldValue(lua_State *L);
LUA_API int objlua_getMethodArgTypes(lua_State *L);
LUA_API int objlua_hasMethod(lua_State *L);
LUA_API int objlua_hasField(lua_State *L);
```
可通过`int lua_compare(lua_State *L, int index1, int index2, int op)`进行`typeof`/`instanceof`比较运算

# 切换语法
```lua
@class_on
class A{}
```
```lua
@class_off
print(class)
```
由于部分Lua组件可能内置Lua脚本源码进行执行（如iuplua），内置脚本使用了`class`/`typeof`/`instanceof`关键字，可通过上述方法切换模式，切换模式会影响当前Lua解析以及设置后的新Lua源码解析时的默认状态（默认开）。

# 演示与字节码解析
### [点我跳转](https://github.com/nwdxlgzs/objlua-bytecode-parse)

# 测试脚本
测试脚本位于`tests/`目录下，在这里你可以看到相关语法、库的测试。[点我跳转](./tests/)

# 协议
- Lua:[MIT](./LUA.LICENSE)
- ObjLua:[MIT](./OBJLUA.LICENSE)
- 尽管MIT协议要求保留相关协议声明，但是作为倾注了大量心血的作品，所以除了遵守上述外，希望您能遵守：
  - 禁止通过删除/隐藏版权声明、项目文档误导性描述或其他方式，暗示或明示本代码为您（使用者）的原创作品。

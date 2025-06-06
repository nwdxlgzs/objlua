#define lobjudata_c
#define LUA_CORE
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "lfunc.h"
#include "lapi.h"
#include "ldebug.h"
#include "lvm.h"
#include "ltable.h"
#include "lobjudata.h"

int Objudata_init(lua_State *L) {
    int ctop = lua_gettop(L);
    if (!luaL_getsubtable(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE)) {
        //ctop+1
        //初始化一个k弱表(类/对象都在这里，不在这里说明只是普通userdata)
        lua_newtable(L); //ctop+2
        lua_pushliteral(L, "k"); //ctop+3
        lua_setfield(L, -2, "__mode"); //ctop+2
        lua_setmetatable(L, -2); //ctop+1
        TString *ts = luaS_new(L, OBJLUA_WEAK_TABLE);
        luaC_fix(L, obj2gco(ts)); //OBJLUA_WEEK_TABLE永不回收，方便我直接luaS_new(L, OBJLUA_WEAK_TABLE)
    }
    // Table* ObjLuaWeakTable = hvalue(s2v(L->top.p-1));
    // luaH_resize(L,ObjLuaWeakTable, 1,160);
    lua_settop(L, ctop); //完成初始化了就结束了，这么做为了方便不用luacall方法直接函数调用，不污染运行栈(f_luaopen里启动运行的)
}

/*
 * 由于解释器以及部分执行时一定以及必然强制产生一些中长期不在堆栈没有Lua范围引用的仅存活在C的堆栈的情况，
 * 为了保证原定架构可以正确定义，这里把对应OP执行通过闭包转换为一个新的一段运行栈区域，
 * 从而更安全稳定的使用luaapi，同时为后续剥离做准备（如果最终决定提供的标准库可以手动构建就能快速封装，并且稳定统一）
 * 闭包方案是最稳妥的，最可控堆栈的方案（参数设置在闭包中，这样定义闭包阶段就可以完成，就不需要作为参数再碰运行栈）
 * 抛出错误用的luaG_runerror是因为RuntimeError
 */
static int ObjudataMT__setup(lua_State *L, int idx);

// //SuperNip永远成功率为0的战术，我GC的最后备手，查看一下有哪些挂载进GC了，GC问题困扰我太久了，还得是完成的打LOG，Lua虚拟机一步一步追踪，十几行代码从头追能追几个小时
// static void SuperNipClass(lua_State *L, LuaObjUData *obj, int line, const char *file) {
//     Table *gct = hvalue(&obj->udata->uv[OBJLUA_UV_gc].uv);
//     printf("===================================\n");
//     printf("On File:%s Line:%d\n", file, line);
//     printf("LuaObjUData->%p\n", obj);
//     printf("alimit->%d\n", gct->alimit);
//     for (int n = 0; n < gct->alimit; n++) {
//         TValue *o = &gct->array[n];
//         if (ttisfulluserdata(o)) {
//             printf("userdata->%p\n", getudatamem(uvalue(o)));
//         } else {
//             printf("TValue->%p type=%s\n", o, ttypename(ttype(o)));
//         }
//     }
//     TValue *o = &obj->udata->uv[OBJLUA_UV_fields].uv;
//     if (!ttisnil(o)) printf("fields->%p\n", getudatamem(uvalue(o)));
//     o = &obj->udata->uv[OBJLUA_UV_methods].uv;
//     if (!ttisnil(o))printf("methods->%p\n", getudatamem(uvalue(o)));
//     o = &obj->udata->uv[OBJLUA_UV_constructors].uv;
//     if (!ttisnil(o))printf("constructors->%p\n", getudatamem(uvalue(o)));
//     o = &obj->udata->uv[OBJLUA_UV_metamethods].uv;
//     if (!ttisnil(o))printf("metamethods->%p\n", getudatamem(uvalue(o)));
//
//     fflush(stdout); // 立即输出
// }
//
// static void SuperNipMethod(lua_State *L, LuaObjMethod *meth) {
//     Table *gct = hvalue(&meth->udata->uv[OBJLUA_UV_gc].uv);
//     printf("===================================\n");
//     printf("LuaObjMethod->%p\n", meth);
//     printf("alimit->%d\n", gct->alimit);
//     for (int n = 0; n < gct->alimit; n++) {
//         TValue *o = &gct->array[n];
//         if (ttisfulluserdata(o)) {
//             printf("userdata->%p\n", getudatamem(uvalue(o)));
//         } else {
//             printf("TValue->%p type=%s\n", o, ttypename(ttype(o)));
//         }
//     }
//     fflush(stdout); // 立即输出
// }


CClosure *RunAtPrepare(lua_State *L, const int nupvals, const lua_CFunction f) {
    CClosure *cl = luaF_newCclosure(L, nupvals);
    cl->f = f;
    for (int n = 0; n < nupvals; n++)
        setnilvalue(&cl->upvalue[n]);
    return cl;
}

/*
 * case OP_DEFCLASS
 * uv1:类的名字
 * uv2:父类（nil为无父类）
 * ret:类
 */
int RunAtOP_DEFCLASS(lua_State *L) {
    int GCIDX = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE); //R1
    //为了防止左脚踩右脚，出现本质clazz->super = clazz，要后注册clazz
    LuaObjUData *clazz = lua_newuserdatauv(L, sizeof(LuaObjUData), LuaObjUDataUpValueMinSize); //R2
    clazz->udata = uvalue(index2value(L, -1)); //R2
    // 预先准备元表
    lua_newtable(L); //R3
    ObjudataMT__setup(L, 3); //R3
    lua_setmetatable(L, -2); //R2
    //创建GC表（承担后续对象GC挂载任务）
    lua_newtable(L); //R3
    lua_pushvalue(L, -1); //R4
    lua_setiuservalue(L, 2, OBJLUA_UV_gc + 1); //R3
    if (lua_isnil(L, lua_upvalueindex(2))) {
        //R3
        //无父类
        clazz->super = NULL; //R3
    } else {
        //有父类
        lua_pushvalue(L, lua_upvalueindex(2)); //R4
        lua_rawget(L, -4); //R4
        if (!lua_toboolean(L, -1)) luaG_runerror(L, "bad super class: not registered"); //R3
        lua_pop(L, 1); //R3
        LuaObjUData *superClass = lua_touserdata(L, lua_upvalueindex(2)); //R3
        if (!superClass->is_class) luaG_runerror(L, "bad super class: not a class"); //R3
        clazz->super = superClass; //R3
        //把父类绑定到现在定义的类的GC表里
        lua_pushvalue(L, lua_upvalueindex(2)); //R4
        lua_rawseti(L, -2, ++GCIDX); //R3
    }
    if (lua_type(L, lua_upvalueindex(1)) == LUA_TSTRING) {
        //R3
        //有名字
        lua_pushvalue(L, lua_upvalueindex(1)); //R4
        TString *ts = tsvalue(index2value(L, -1)); //R4
        clazz->name = ts; //R4
        //把父类绑定到现在定义的类的GC表里
        lua_rawseti(L, -2, ++GCIDX); //R3
    } else {
        //无名字
        clazz->name = NULL;
    }
    clazz->classholder = clazz; //类就是自己，这时候就不需要挂载GC了
    clazz->is_class = 1;
    clazz->size_constructors = 0;
    clazz->size_metamethods = 0;
    clazz->size_fields = 0;
    clazz->constructors = NULL;
    clazz->metamethods = NULL;
    clazz->fields = NULL;
    clazz->size_methods = 0;
    clazz->methods = NULL;
    clazz->size_abstractmethods = 0;
    clazz->abstractmethods = NULL;
    //完成了，可以注册了
    lua_pushvalue(L, 2); //R4 键
    lua_pushboolean(L, 1); //R5 值
    lua_rawset(L, -5); //R3
    //回到clazz位置，返回1个（也就是clazz）
    lua_settop(L, 2); //R0
    return 1;
}

/*
 * case OP_DEFFIELD
 * uv1:类
 * uv2:字段名
 * uv3:字段值
 * uv4:字段访问标志
 * uv5:字段是否初始化
 */
int RunAtOP_DEFFIELD(lua_State *L) {
    int GCIDX = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE); //R1
    //首先看看类是不是合法的
    lua_pushvalue(L, lua_upvalueindex(1)); //R2
    lua_rawget(L, -2); //R2
    if (!lua_toboolean(L, -1)) luaG_runerror(L, "define class field failed: not a class in "OBJLUA_WEAK_TABLE); //R2
    lua_pop(L, 1); //R1
    LuaObjUData *clazz = lua_touserdata(L,lua_upvalueindex(1)); //R1
    if (!clazz->is_class) luaG_runerror(L, "define class field failed: target is object");
    const TValue *nameT = index2value(L,lua_upvalueindex(2)); //R1
    if (lua_type(L, lua_upvalueindex(2)) != LUA_TSTRING)
        luaG_runerror(L, "define class field failed: field name must be string"); //R1
    int deffield_access = 1;
    //不能定义父类存在的字段
    LuaObjUData *super = clazz;
    TString *nameS = tsvalue(nameT);
    while (super) {
        for (int j = 0; j < super->size_fields; ++j) {
            LuaObjField *super_field = super->fields[j];
            if (luaS_streq(super_field->name, nameS)) {
                deffield_access = 0;
                break;
            }
        }
        super = super->super;
    }
    if (!deffield_access)
        luaG_runerror(L, "define class field failed: field '%s' already exists in '%s' class",
                      getstr(tsvalue(nameT)), super == clazz ? "self" : "super");
    LuaObjField *field = (LuaObjField *) lua_newuserdatauv(L, sizeof(LuaObjField), LuaObjFieldUpValueMinSize); //R2
    field->name = tsvalue(nameT);
    field->self = clazz;
    LuaObjAccessFlags flags = lua_tointeger(L, lua_upvalueindex(4)); //R2
    field->flags = flags;
    field->initconst = 0;
    field->udata = uvalue(index2value(L, -1)); //R2
    //创建GC表（承担后续对象GC挂载任务）
    lua_newtable(L); //R3
    lua_pushvalue(L, -1); //R4
    lua_setiuservalue(L, -3, OBJLUA_UV_gc + 1); //R3
    //字段名GC在字段的GC表里
    lua_pushvalue(L, lua_upvalueindex(2)); //R4
    lua_rawseti(L, -2, ++GCIDX); //R3
    //同样的，绑定关系的类也需要绑定在GC表里
    lua_pushvalue(L, lua_upvalueindex(1)); //R4
    lua_rawseti(L, -2, ++GCIDX); //R3
    //字段自己本身应该绑在clazz的GC表里，并且添加进列表，这一步进行封装使用
    lua_pushcfunction(L, Objudata_DefField); //R4 需要三个参数:clazz,field,deffield
    lua_pushvalue(L,lua_upvalueindex(1)); //R5 clazz
    lua_pushvalue(L, -4); //R6 field
    lua_pushboolean(L, 1); //R7
    lua_call(L, 3, 0); //R3
    if (lua_toboolean(L, lua_upvalueindex(5))) {
        //R3
        //R3
        //需要设置初始值
        lua_pushvalue(L, lua_upvalueindex(3)); //R4
        lua_setiuservalue(L, -3, OBJLUA_UV_fields + 1); //R3
        //const的禁止再赋值
        if (flags & LUAOBJ_ACCESS_CONST)field->initconst = 1;
    } else {
        lua_pushnil(L); //R4
        lua_setiuservalue(L, -3, OBJLUA_UV_fields + 1); //R3
    }
    return 0;
}

/*
 * case OP_DEFMETHODARGTYPE
 * uv1:方法
 * uv2:类型（字符串或者userdata<LuaObjUData>）
 * uv3:类型模式
 * uv4:定义的偏移量
 */
int RunAtOP_DEFMETHODARGTYPE(lua_State *L) {
    if (lua_type(L, lua_upvalueindex(1)) != LUA_TUSERDATA) //R0
        luaG_runerror(L, "method arg type define: it just works on userdata<LuaObjMethod>");
    const LuaObjMethod *method = (LuaObjMethod *) lua_touserdata(L,lua_upvalueindex(1)); //R0
    const int arg_pos = lua_tointeger(L, lua_upvalueindex(4)); //R0
    MethodArgType *mtype = method->argtypes[arg_pos];
    const int typeflags = lua_tointeger(L, lua_upvalueindex(3)); //R0
    if (typeflags == 0) {
        mtype->none = 1;
    } else if (typeflags & TYPEMASK_is_vararg) {
        mtype->is_vararg = 1;
    } else if (typeflags & TYPEMASK_is_typemode) {
        if (lua_type(L, lua_upvalueindex(2)) != LUA_TSTRING) //R0
            luaG_runerror(L, "method arg type define: typemode need string");
        mtype->is_typemode = 1;
        //绑定GC到MethodArgType
        lua_pushnil(L); //R1
        setuvalue(L, index2value(L,-1), mtype->udata); //R1
        lua_pushvalue(L, lua_upvalueindex(2)); //R2
        mtype->type = tsvalue(index2value(L, -1)); //R2
        lua_setiuservalue(L, -2, OBJLUA_UV_gc + 1); //R1
        lua_pop(L, 1);
        // lua_setiuservalue(L, lua_upvalueindex(1), OBJLUA_UV_gc+1); //R0
    } else if (typeflags & TYPEMASK_is_classmode) {
        if (lua_type(L, lua_upvalueindex(2)) != LUA_TUSERDATA) {
            //R0
        badclassmode:;
            luaG_runerror(L, "method arg type define: classmode need userdata<LuaObjUData>");
        }
        lua_getfield(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE); //R1
        //是不是合法的
        lua_pushvalue(L, lua_upvalueindex(2)); //R2
        lua_rawget(L, -2); //R2
        if (!lua_toboolean(L, -1)) goto badclassmode; //R2
        lua_pop(L, 2); //R0
        mtype->is_classmode = 1;
        //绑定GC到MethodArgType
        lua_pushnil(L); //R1
        setuvalue(L, index2value(L,-1), mtype->udata); //R1
        lua_pushvalue(L, lua_upvalueindex(2)); //R2
        mtype->clazz = (LuaObjUData *) lua_touserdata(L, -1); //R2
        lua_setiuservalue(L, -2, OBJLUA_UV_gc + 1); //R1
        lua_pop(L, 1);
        // lua_setiuservalue(L, lua_upvalueindex(1), OBJLUA_UV_gc+1); //R0
    } else luaG_runerror(L, "method arg type define: unknown typeflags");
    return 0;
}

/*
 * case OP_DEFMETHOD
 * uv1:类
 * uv2:方法名
 * uv3:方法实现函数
 * uv4:方法访问标志
 * uv5:nargs
 * ret:方法对象（userdata<LuaObjMethod>）
 */
int RunAtOP_DEFMETHOD(lua_State *L) {
    int GCIDX = 0;
    lua_getfield(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE); //R1
    //首先看看类是不是合法的
    lua_pushvalue(L, lua_upvalueindex(1)); //R2
    lua_rawget(L, -2); //R2
    if (!lua_toboolean(L, -1)) luaG_runerror(L, "define class method failed: not a class in "OBJLUA_WEAK_TABLE); //R2
    lua_pop(L, 2); //R0
    LuaObjUData *clazz = lua_touserdata(L, lua_upvalueindex(1)); //R0
    if (!clazz->is_class) luaG_runerror(L, "define class method failed: target is object");
    if (lua_type(L, lua_upvalueindex(2)) != LUA_TSTRING) //R0
        luaG_runerror(L, "define class method failed: method name must be string");
    LuaObjMethod *method = lua_newuserdatauv(L, sizeof(LuaObjMethod), LuaObjMethodUpValueMinSize); //R1
    method->udata = uvalue(index2value(L, -1)); //R1
    const LuaObjAccessFlags flags = lua_tointeger(L, lua_upvalueindex(4)); //R1
    method->flags = flags;
    method->self = clazz;
    method->name = tsvalue(index2value(L, lua_upvalueindex(2))); //R1
    if (flags & LUAOBJ_ACCESS_ABSTRACT) method->func = NULL;
    else method->func = clLvalue(index2value(L, lua_upvalueindex(3))); //R1
    method->argtypes = NULL;
    const int nargs = lua_tointeger(L, lua_upvalueindex(5)); //R1
    method->nargs = nargs;
    //创建GC表（承担后续对象GC挂载任务）
    lua_newtable(L); //R2
    lua_pushvalue(L, -1); //R3
    lua_setiuservalue(L, -3, OBJLUA_UV_gc + 1); //R2
    //类/方法名/函数都挂载到GC表里
    lua_pushvalue(L, lua_upvalueindex(1)); //R3 clazz
    lua_rawseti(L, -2, ++GCIDX); //R2
    lua_pushvalue(L, lua_upvalueindex(2)); //R3 method name
    lua_rawseti(L, -2, ++GCIDX); //R2
    if (!(flags & LUAOBJ_ACCESS_ABSTRACT)) {
        //抽象方法对应nil这就别捣乱了
        lua_pushvalue(L, lua_upvalueindex(3)); //R3 method func
        lua_rawseti(L, -2, ++GCIDX); //R2
    }
    if (flags & LUAOBJ_ACCESS_CONSTRUCTOR) {
        //R2
        //构造函数
        //构造函数自己本身应该绑在clazz的GC表里，并且添加进列表，这一步进行封装使用
        lua_pushcfunction(L, Objudata_DefConstructor); //R3 需要两个参数:clazz,constructor
        lua_pushvalue(L,lua_upvalueindex(1)); //R4 clazz
        lua_pushvalue(L, -4); //R5 constructor
        lua_call(L, 2, 0); //R2
    } else if (flags & LUAOBJ_ACCESS_META) {
        //元方法
        lua_pushcfunction(L, Objudata_DefMetaMethod); //R3 需要三个参数:clazz,method,method_name
        lua_pushvalue(L,lua_upvalueindex(1)); //R4 clazz
        lua_pushvalue(L, -4); //R5 method
        lua_pushvalue(L, lua_upvalueindex(2)); //R6 method_name
        lua_call(L, 3, 0); //R2
    } else if (flags & LUAOBJ_ACCESS_ABSTRACT) {
        //抽象方法函数
        //抽象方法和方法基本一样，但是没了函数体
        lua_pushcfunction(L, Objudata_DefAbstractMethod); //R3 需要两个参数:clazz,method
        lua_pushvalue(L,lua_upvalueindex(1)); //R4 clazz
        lua_pushvalue(L, -4); //R5 method
        lua_call(L, 2, 0); //R2
    } else {
        //方法函数
        //方法函数自己本身应该绑在clazz的GC表里，并且添加进列表，这一步进行封装使用
        lua_pushcfunction(L, Objudata_DefMethod); //R3 需要两个参数:clazz,method
        lua_pushvalue(L,lua_upvalueindex(1)); //R4 clazz
        lua_pushvalue(L, -4); //R5 method
        lua_call(L, 2, 0); //R2
    }
    GCIDX = luaL_len(L, -1); //GCIDX内部可能已经被添加
    if (nargs > 0) {
        MethodArgType **argtypes = lua_newuserdatauv(L, nargs * sizeof(MethodArgType *), 0); //R3
        memset(argtypes, 0, nargs * sizeof(MethodArgType *));
        method->argtypes = argtypes;
        //MethodArgType**生命周期交给method
        lua_rawseti(L, -2, ++GCIDX); //R2
        //初始化
        for (int j = 0; j < nargs; ++j) {
            MethodArgType *mtype = lua_newuserdatauv(L, sizeof(MethodArgType), MethodArgTypeUpValueMinSize); //R3
            memset(mtype, 0, sizeof(MethodArgType));
            mtype->udata = uvalue(index2value(L, -1)); //R3
            argtypes[j] = mtype;
            //MethodArgType*生命周期交给method
            lua_rawseti(L, -2, ++GCIDX);
        }
    }
    lua_settop(L, 1); //R1
    return 1; //返回方法
}


static int ObjudataMT__tostring(lua_State *L) {
    LuaObjUData *classOrObj = (LuaObjUData *) lua_touserdata(L, 1);
    lua_pushfstring(L, "%s[%s]: %p", classOrObj->is_class ? "class" : "object",
                    classOrObj->name ? getstr(classOrObj->name) : "<anonymous>",
                    lua_topointer(L, 1));
    return 1;
}

#define ObjWeakTableGet(L) \
    TValue *ObjLuaWeakTable = NULL;\
    if (!luaV_fastget(L, &G(L)->l_registry, luaS_newliteral(L, OBJLUA_WEAK_TABLE), ObjLuaWeakTable, (TValue *) luaH_getshortstr)) {\
    luaG_runerror(L, "'f_luaopen' not worked: <"OBJLUA_WEAK_TABLE"> is not initialized");\
    }

#ifndef isvalid
#define isvalid(L, o)	(!ttisnil(o) || o != &G(L)->nilvalue)
#endif
static int
verify_type(lua_State *L, LuaObjMethod *constructor, MethodArgType **types, MethodArgType *last,
            int absLowReg,
            TValue *ObjLuaWeakTable) {
    TValue *slot;
    int checknargs = last->is_vararg ? constructor->nargs - 1 : constructor->nargs;
    int match = 1;
    for (int j = 0; j < checknargs; ++j) {
        MethodArgType *type = types[j];
        if (type->none) continue; //没限制类型
        if (type->is_typemode) {
            //any类型提前转换到none=1了，虽然是typemode格式定义的，但是不归typemode管
            TValue *o = index2value(L, absLowReg + j);
            int typeval = isvalid(L, o) ? ttype(o) : LUA_TNONE;
            char *typestr = ttypename(typeval);
            char *wish = getstr(type->type);
            if (strcmp(typestr, wish) != 0) {
                match = 0;
                break;
            }
        } else if (type->is_classmode) {
            //无论是类还是对象，都是注册进弱表的，检查一下就知道数据是否合法
            TValue *o = index2value(L, absLowReg + j);
            if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get)) {
                match = 0;
                break;
            }
            //这时候检查类型（当然也可能是父类符合，这都算）
            LuaObjUData *clazz = (LuaObjUData *) getudatamem(uvalue(o));
            LuaObjUData *wish = type->clazz;
            int submatch = 0;
            while (clazz != NULL) {
                //NULL是顶层了
                if (wish->classholder == clazz->classholder) {
                    submatch = 1;
                    break;
                }
                clazz = clazz->super;
            }
            if (!submatch) {
                match = 0;
                break;
            }
        } else {
            match = 0;
            break;
        }
    }
    return match;
}

/*
 * 根据高低寄存器中间内容自适应选项正确的方法（多态）
 * name 方法名。构造器模式此参数无效果，其他情况为必须参数
 * absLowReg/absHighReg 寄存器绝对位置，满足absHighReg<absLowReg时认为无参数
 * constructor_mode 构造器模式，只搜索构造函数，此时要求只能是类不能是对象
 * include_super 是否包含父类，默认不包含。构造器模式此参数无效果
 * metamethod_mode 是否是元方法模式，是就在method寻找逻辑中使用metamethod的数据找
 * 此函数不检查private
 */
static LuaObjMethod *
polymorphism_overload_method(lua_State *L, TString *name, int absLowReg, int absHighReg, LuaObjUData *classOrObj,
                             lu_byte constructor_mode,
                             lu_byte include_super, lu_byte metamethod_mode) {
    if (!constructor_mode && !name)
        luaG_runerror(L, "polymorphism method need name.");
    LuaObjMethod *method = NULL;
    if (absLowReg < 0 || absHighReg < 0) return method;
    ObjWeakTableGet(L);
    int argCount = 0;
    if (absLowReg <= absHighReg) {
        argCount = absHighReg - absLowReg + 1;
    }
    if (constructor_mode) {
        //第一遍遍历，先把有定义类型的构造函数分出来
        for (size_t i = 0; i < classOrObj->size_constructors; ++i) {
            LuaObjMethod *constructor = classOrObj->constructors[i];
            MethodArgType **types = constructor->argtypes;
            if (!types) continue;
            //检查最后一个是不是is_vararg，不是就直接比较长度（无参数的不是多态，也就是说nargs>=1）
            MethodArgType *last = types[constructor->nargs - 1];
            if (!last->is_vararg && constructor->nargs != argCount) continue; //快速跳过不定长方法长度不匹配的
            if (verify_type(L, constructor, types, last, absLowReg, ObjLuaWeakTable))
                return constructor; //第一优先原则，找到就不找更符合的了
        }
        //第二遍遍历，把第一个没有类型要求的构造函数找出来
        for (size_t i = 0; i < classOrObj->size_constructors; ++i) {
            LuaObjMethod *constructor = classOrObj->constructors[i];
            if (!constructor->argtypes) return constructor; //找到了，直接返回
        }
        return NULL; //返回NULL，不报错
    } else {
        //第一遍遍历，先把有定义类型的函数分出来
        size_t size_methods = classOrObj->size_methods;
        LuaObjMethod **methods = classOrObj->methods;
        if (metamethod_mode) {
            size_methods = classOrObj->size_metamethods;
            methods = classOrObj->metamethods;
        }
        for (size_t i = 0; i < size_methods; ++i) {
            //匹配方法名
            method = methods[i];
            if (luaS_streq(name, method->name)) {
                MethodArgType **types = method->argtypes;
                if (!types) continue;
                //检查最后一个是不是is_vararg，不是就直接比较长度（无参数的不是多态，也就是说nargs>=1）
                MethodArgType *last = types[method->nargs - 1];
                MethodArgType *fst = types[0];
                if (!last->is_vararg && method->nargs != argCount) continue; //快速跳过不定长方法长度不匹配的
                if (verify_type(L, method, types, last, absLowReg, ObjLuaWeakTable)) return method; //第一优先原则，找到就不找更符合的了
            }
        }
        //第二遍遍历，把第一个没有类型要求的构造函数找出来
        for (size_t i = 0; i < size_methods; ++i) {
            //匹配方法名
            method = methods[i];
            if (luaS_streq(name, method->name)) {
                if (!method->argtypes) return method; //找到了，直接返回
            }
        }
        if (include_super && classOrObj->super) {
            method = polymorphism_overload_method(L, name, absLowReg, absHighReg, classOrObj->super, constructor_mode,
                                                  include_super, metamethod_mode);
            if (method) return method;
        }
        return NULL; //返回NULL，不报错
    }
}


/*
 * 抽象__call，如果索引到方法，通过这个函数完成代理，提供抽象函数
 * 因为多态只有调用才知道是哪个方法
 * 第一个上值存储对象或者类自己
 * 第二个上值存储方法名字，nil时为构建器
 * 第三个是根据__index期间确定提供方法的对象或者类（自己或者父类都有可能）
 */
static int ObjudataMT__abstractcall(lua_State *L) {
    LuaObjUData *classOrObj = (LuaObjUData *) lua_touserdata(L, lua_upvalueindex(1));
    LuaObjUData *methodClassOrObj = (LuaObjUData *) lua_touserdata(L, lua_upvalueindex(3));
    int nargs = lua_gettop(L);
    int have_access = 0;
    CallInfo *lastCall = L->ci;
    if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到Lua函数层
    if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到MethodWrapCall层
    if (lastCall && ttypetag(s2v(lastCall->func.p)) == LUA_VCCL) {
        CClosure *wrapcall = clCvalue(s2v(lastCall->func.p));
        if ((Objudata_MethodWrapCall == wrapcall->f ||
             Objudata_metaProxy == wrapcall->f)
            && wrapcall->nupvalues >= 2) {
            //检查是不是类内调用
            LuaObjUData *classobj = (LuaObjUData *) getudatamem(uvalue(&wrapcall->upvalue[0]));
            if (classobj == classOrObj || classobj->classholder == classOrObj->classholder)
                have_access = 1;
        }
    }
    if (lua_isnil(L, lua_upvalueindex(2))) {
        //构建器
        if (classOrObj->is_class || !have_access) luaG_runerror(L, "constructor pre check failed.");
        LuaObjMethod *constructor = polymorphism_overload_method(L, NULL, 1, nargs, methodClassOrObj, 1, 0, 0);
        if (!constructor) luaG_runerror(L, "constructor not found");
        LuaObjAccessFlags flags = constructor->flags;
        if (flags & LUAOBJ_ACCESS_PUBLIC) {
        constructor_call:;
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushnil(L);
            TValue *o = index2value(L, -1);
            setuvalue(L, o, constructor->udata);
            lua_pushcclosure(L, Objudata_MethodWrapCall, 2);
            for (int i = 1; i <= nargs; ++i) {
                lua_pushvalue(L, i);
            }
            lua_call(L, nargs, LUA_MULTRET);
            return lua_gettop(L) - nargs;
        } else if (flags & LUAOBJ_ACCESS_PRIVATE) {
            //检查过了
            // if (!have_access) luaG_runerror(L, "private constructor cannot be accessed");
            goto constructor_call;
        } else luaG_runerror(L, "method not have public or private access");
    } else {
        TValue *methodNameO = index2value(L, lua_upvalueindex(2));
        TString *methodName = tsvalue(methodNameO);
        //之前就已经确定了的最搞出现这个函数的类，不用从最叶子开始找，当然多态问题可能不是当前层可能还在super，所以还得包括super
        LuaObjMethod *method = polymorphism_overload_method(L, methodName, 1, nargs, methodClassOrObj, 0, 1, 0);
        // 不信赖methodClassOrObj那就从classOrObj从头找，除非中途增加新方法，但是这对吗？这不对吧，增加方法说明还是定义阶段，定义阶段哪里来的CallMethod
        // LuaObjMethod *method = polymorphism_overload_method(L, methodName, 1, nargs, classOrObj, 0, 1, 0);
        if (!method) {
            method = polymorphism_overload_method(L, methodName, 1, nargs, methodClassOrObj, 0, 1, 0);
            luaG_runerror(L, "method '%s' not found",getstr(methodName));
        }
        LuaObjAccessFlags flags = method->flags;
        if (flags & LUAOBJ_ACCESS_PUBLIC) {
        do_call:;
            lua_pushvalue(L, lua_upvalueindex(1));
            lua_pushnil(L);
            TValue *o = index2value(L, -1);
            setuvalue(L, o, method->udata);
            lua_pushcclosure(L, Objudata_MethodWrapCall, 2);
            for (int i = 1; i <= nargs; ++i) {
                lua_pushvalue(L, i);
            }
            lua_call(L, nargs, LUA_MULTRET);
            return lua_gettop(L) - nargs;
        } else if (flags & LUAOBJ_ACCESS_PRIVATE) {
            if (!have_access) luaG_runerror(L, "private method '%s' cannot be accessed", getstr(methodName));
            goto do_call;
        } else luaG_runerror(L, "method not have public or private access");
    }
}

static int ObjudataMT__indexImpl(lua_State *L, LuaObjUData *origin, LuaObjUData *classOrObj, TString *key) {
    int have_access = 0;
    CallInfo *lastCall = L->ci;
    if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到Lua函数层
    if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到MethodWrapCall层
    if (lastCall && ttypetag(s2v(lastCall->func.p)) == LUA_VCCL) {
        CClosure *wrapcall = clCvalue(s2v(lastCall->func.p));
        if ((Objudata_MethodWrapCall == wrapcall->f ||
             Objudata_metaProxy == wrapcall->f)
            && wrapcall->nupvalues >= 2) {
            //检查是不是类内调用
            LuaObjUData *classobj = (LuaObjUData *) getudatamem(uvalue(&wrapcall->upvalue[0]));
            if (classobj == origin || classobj->classholder == origin->classholder)
                have_access = 1;
        }
    }
    LuaObjAccessFlags flags;
    //遍历fields
    for (size_t i = 0; i < classOrObj->size_fields; ++i) {
        LuaObjField *field = classOrObj->fields[i];
        if (luaS_streq(field->name, key)) {
            flags = field->flags;
            if (flags & LUAOBJ_ACCESS_PUBLIC) {
            index_field:;
                if (!(flags & LUAOBJ_ACCESS_STATIC) && origin->is_class)
                    luaG_runerror(L, "object field '%s' cannot be accessed as static", getstr(key));
                lua_pushnil(L);
                setobj2n(L, index2value(L, -1), &field->udata->uv[OBJLUA_UV_fields].uv);
                return 1;
            } else if (flags & LUAOBJ_ACCESS_PRIVATE) {
                if (!have_access) luaG_runerror(L, "private field '%s' cannot be accessed", getstr(key));
                goto index_field;
            } else luaG_runerror(L, "field not have public or private access");
        }
    }
    //遍历methods
    LuaObjMethod *method;
    for (size_t i = 0; i < classOrObj->size_methods; ++i) {
        method = classOrObj->methods[i];
        flags = method->flags;
        if (flags & LUAOBJ_ACCESS_PRIVATE && !have_access) continue;
        if (!(flags & LUAOBJ_ACCESS_STATIC) && origin->is_class) continue;
        if (luaS_streq(method->name, key)) {
            //肯定不能直接返回这个方法，因为多态，返回一个代理函数，干__call的活，abstractcall传origin
            lua_pushnil(L);
            setuvalue(L, index2value(L, -1), origin->udata);
            lua_pushnil(L);
            setsvalue2n(L, index2value(L, -1), key);
            lua_pushnil(L);
            setuvalue(L, index2value(L, -1), classOrObj->udata);
            lua_pushcclosure(L, ObjudataMT__abstractcall, 3);
            return 1;
        }
    }
    if (have_access && !origin->is_class && luaS_streq(classOrObj->name, key)) {
        //构建函数调用另一个构建函数共同初始化
        lua_pushnil(L);
        setuvalue(L, index2value(L, -1), origin->udata);
        lua_pushnil(L);
        lua_pushnil(L);
        setuvalue(L, index2value(L, -1), classOrObj->udata);
        lua_pushcclosure(L, ObjudataMT__abstractcall, 3);
        return 1;
    }
    //没找到，试试父类继续往前找
    if (classOrObj->super) {
        LuaObjUData *super = classOrObj->super;
        return ObjudataMT__indexImpl(L, origin, super, key);
    } else
        luaG_runerror(L, "field/method '%s' not found", getstr(key));
    return 0;
}

static int ObjudataMT__index(lua_State *L) {
    LuaObjUData *classOrObj = (LuaObjUData *) lua_touserdata(L, 1);
    luaL_checktype(L, 2, LUA_TSTRING);
    TString *key = tsvalue(index2value(L, 2));
    return ObjudataMT__indexImpl(L, classOrObj, classOrObj, key);
}

static int ObjudataMT__newindex(lua_State *L) {
    lua_settop(L, 3); //R3
    luaL_checktype(L, 1, LUA_TUSERDATA); //R3
    luaL_checktype(L, 2, LUA_TSTRING); //R3
    LuaObjUData *clazz = (LuaObjUData *) lua_touserdata(L, 1); //R3
    TString *key = tsvalue(index2value(L, 2)); //R3
    LuaObjUData *curClass = clazz;
retry:;
    //遍历fields
    for (size_t i = 0; i < curClass->size_fields; ++i) {
        LuaObjField *field = curClass->fields[i];
        if (luaS_streq(field->name, key)) {
            if (field->flags & LUAOBJ_ACCESS_CONST && field->initconst)
                luaG_runerror(L, "const field '%s' cannot be modified", getstr(key));
            LuaObjAccessFlags flags = field->flags;
            if (flags & LUAOBJ_ACCESS_PUBLIC) {
            doset_field:;
                if (!(flags & LUAOBJ_ACCESS_STATIC) && clazz->is_class) {
                    luaG_runerror(L, "object field '%s' cannot be modified", getstr(key));
                }
                lua_pushnil(L); //R4
                setuvalue(L, index2value(L, -1), field->udata); //R4
                lua_pushvalue(L, -2); //R5
                lua_setiuservalue(L, -2, OBJLUA_UV_fields + 1); //R4
                lua_pop(L, 1); //R3
                if (field->flags & LUAOBJ_ACCESS_CONST) field->initconst = 1;
                return 0;
            } else if (flags & LUAOBJ_ACCESS_PRIVATE) {
                int have_access = 0;
                CallInfo *lastCall = L->ci;
                if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到Lua函数层
                if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到MethodWrapCall层
                if (lastCall && ttypetag(s2v(lastCall->func.p)) == LUA_VCCL) {
                    CClosure *wrapcall = clCvalue(s2v(lastCall->func.p));
                    if ((Objudata_MethodWrapCall == wrapcall->f ||
                         Objudata_metaProxy == wrapcall->f)
                        && wrapcall->nupvalues >= 2) {
                        //检查是不是类内调用
                        LuaObjUData *classobj = (LuaObjUData *) getudatamem(uvalue(&wrapcall->upvalue[0]));
                        if (classobj == clazz || classobj->classholder == clazz->classholder)
                            have_access = 1;
                    }
                }
                if (!have_access) luaG_runerror(L, "private field '%s' cannot be accessed", getstr(key));
                goto doset_field;
            } else luaG_runerror(L, "field not have public or private access");
        }
    }
    //如果是顶层对象或者类，结束
    if (curClass->super == NULL) {
        //__newindex是不支持对非字段的操作的
        luaG_runerror(L, "field '%s' not found", getstr(key));
    } else {
        //去父类找
        curClass = curClass->super;
        goto retry;
    }
    return 0;
}

static LuaObjUData *makeObject(lua_State *L, LuaObjUData *clazz, TValue *ObjLuaWeakTable, int absLowReg,
                               int absHighReg) {
    int GCIDX = 0;
    int argCount = 0;
    if (absLowReg <= absHighReg) {
        argCount = absHighReg - absLowReg + 1;
    }
    LuaObjUData *obj = lua_newuserdatauv(L, sizeof(LuaObjUData), LuaObjUDataUpValueMinSize); //X+1
    int retTop = lua_gettop(L); //X+1
    obj->udata = uvalue(index2value(L, -1)); //X+1
    //预先准备元表
    lua_newtable(L); //X+2
    ObjudataMT__setup(L, retTop + 1); //X+2
    lua_setmetatable(L, -2); //X+1
    //创建GC表（承担后续对象GC挂载任务）
    lua_newtable(L); //X+2
    lua_pushvalue(L, -1); //X+3
    lua_setiuservalue(L, -3, OBJLUA_UV_gc + 1); //X+2
    //clazz挂载进obj的GC
    lua_pushnil(L); //X+3
    setuvalue(L, index2value(L, -1), clazz->udata); //X+3
    lua_rawseti(L, -2, ++GCIDX); //X+2
    obj->name = clazz->name; //这时候name通过clazz绑定，clazz绑着obj，就不需要单独绑了
    obj->classholder = clazz;
    obj->is_class = 0; //不是类
    obj->size_constructors = clazz->size_constructors;
    obj->constructors = clazz->constructors;
    obj->size_methods = clazz->size_methods;
    obj->methods = clazz->methods;
    if (clazz->super) {
        LuaObjUData *super_class = clazz->super;
        lua_pushnil(L); //X+3
        setuvalue(L, index2value(L, -1), super_class->udata); //X+3
        for (int i = absLowReg; i <= absHighReg; ++i) {
            lua_pushvalue(L, i);
        } //X+3+argCount
        //call，得到super对象
        lua_call(L, argCount, 1); //X+3
        obj->super = lua_touserdata(L, -1); //X+3
        GCIDX = luaL_len(L, -2); //GCIDX内部可能已经被添加
        //挂载父对象到自己GC
        lua_rawseti(L, -2, ++GCIDX); //X+2
    } else {
        //自己就是顶级class
        obj->super = NULL;
    }
    //元方法需要重新自定义绑定……而且因为元表问题只能错后在定义元表之后，所以干脆都做完之后再设置，现在X+2
    //因为字段需要，提高到字段定义前面，不是最最后
    obj->size_metamethods = 0;
    obj->metamethods = NULL;
    for (size_t i = 0; i < clazz->size_metamethods; ++i) {
        LuaObjMethod *metamethod = clazz->metamethods[i];
        lua_pushcfunction(L, Objudata_DefMetaMethod); //X+3 需要三个参数:clazz,method,method_name
        lua_pushvalue(L, -3); //X+4 obj
        lua_pushnil(L); //X+5
        setuvalue(L, index2value(L, -1), metamethod->udata); //X+5 method
        lua_pushnil(L); //X+6
        setsvalue2n(L, index2value(L, -1), metamethod->name); //X+6 method_name
        lua_call(L, 3, 0); //X+2
    }
    obj->size_abstractmethods = 0;
    obj->abstractmethods = NULL;
    //字段很特殊，不能像方法一样直接复制，非static的字段那就是独立的……
    obj->size_fields = 0;
    obj->fields = NULL;
    for (size_t i = 0; i < clazz->size_fields; ++i) {
        LuaObjField *field = clazz->fields[i];
        const LuaObjAccessFlags flags = field->flags;
        if (flags & LUAOBJ_ACCESS_STATIC) {
            //最方便的一个情况
            lua_pushcfunction(L, Objudata_DefField); //X+3 需要三个参数:clazz,field,deffield
            lua_pushvalue(L, -3); //X+4 obj
            lua_pushnil(L); //X+5
            setuvalue(L, index2value(L, -1), field->udata); //X+5 field
            lua_pushboolean(L, 0); //X+6
            lua_call(L, 3, 0); //X+2
        } else {
            int curTop = lua_gettop(L); //Y
            int FGCIDX = 0;
            LuaObjField *obj_field = lua_newuserdatauv(L, sizeof(LuaObjField), LuaObjFieldUpValueMinSize); //Y+1
            obj_field->name = field->name;
            obj_field->self = obj;
            obj_field->flags = field->flags;
            obj_field->initconst = field->initconst;
            obj_field->udata = uvalue(index2value(L, -1)); //Y+1
            if (obj_field->flags & LUAOBJ_ACCESS_NOWRAP) {
                ///旧版方案：动态字段初始值直接从原来的拷贝一份
            setnowrap:;
                lua_pushnil(L); //Y+2
                setobjt2t(L, index2value(L, -1), &field->udata->uv[OBJLUA_UV_fields].uv); //Y+2
            } else {
                ///新版将会自动调用一次字段的函数
                if (ttype(&field->udata->uv[OBJLUA_UV_fields].uv) == LUA_TFUNCTION) {
                    //Y+1
                    //如果是函数，说明需要执行一次才能得到内容

                    // * upval[2]:LuaObjMethod/LClosure
                    lua_pushvalue(L, retTop); //Y+2 临时未完成初始化的对象
                    lua_pushnil(L); // Y+3
                    setobjt2t(L, index2value(L, -1), &field->udata->uv[OBJLUA_UV_fields].uv); //Y+3
                    lua_pushcclosure(L, Objudata_MethodWrapCall, 2); //Y+2
                    lua_call(L, 0, 1); //Y+1
                } else goto setnowrap;
            }
            lua_setiuservalue(L, -2, OBJLUA_UV_fields + 1); //Y+1

            // lua_pushnil(L); //T+2
            // lua_setiuservalue(L, -2, OBJLUA_UV_fields + 1); //Y+1
            //创建GC表（承担后续对象GC挂载任务）
            lua_newtable(L); //Y+2
            lua_pushvalue(L, -1); //Y+3
            lua_setiuservalue(L, -3, OBJLUA_UV_gc + 1); //Y+2
            //字段名在field已经绑了，InstanceField->Object->Class->ClassField一路指向，所以只需要绑self，偷点懒
            //同样的，绑定关系的类也需要绑定在GC表里
            lua_pushvalue(L, retTop); //Y+3 obj
            lua_rawseti(L, -2, ++FGCIDX); //Y+2
            //字段自己本身应该绑在clazz的GC表里，并且添加进列表，这一步进行封装使用
            lua_pushcfunction(L, Objudata_DefField); //Y+3 需要三个参数:clazz,field,deffield
            lua_pushvalue(L, retTop); //Y+4 clazz
            lua_pushvalue(L, -4); //Y+5 field
            lua_pushboolean(L, 1); //Y+6
            lua_call(L, 3, 0); //Y+2
            lua_settop(L, curTop); //Y=X+2
        }
    }
    // lua_pop(L, 1); //剩下GC绑定有相关内部代码完成，这个GC表就可以弹出了
    lua_settop(L, retTop); //X+1
    //都初始化完毕，挂载到弱表
    lua_getfield(L, LUA_REGISTRYINDEX, OBJLUA_WEAK_TABLE); //X+2
    lua_pushvalue(L, -2); //X+3 obj
    lua_pushboolean(L, 1); //X+4
    lua_rawset(L, -3); //X+2
    lua_pop(L, 1); //X+1
    return obj;
}


/*
 * 顶级Class:字段|方法|名字的GC，回收交给自然的对外的访问，挂载弱表
 * 顶级Object:字段|方法|名字没有挂载GC，直接用顶级Class，所以把顶级Class挂载自己的GC，回收交给自然的对外的访问，挂载弱表
 * 子Class:字段|方法|名字挂载GC，同时把super挂载进GC保持引用，回收交给自然的对外的访问，挂载弱表
 * 子Object:字段|方法|名字没有挂载GC，直接用子Class，所以把super挂载自己的GC，回收交给自然的对外的访问，挂载弱表
 */
static int ObjudataMT__call(lua_State *L) {
    LuaObjUData *clazz = (LuaObjUData *) lua_touserdata(L, 1);
    if (!clazz->is_class) luaG_runerror(L, "only class can call constructors");
    int nargs = lua_gettop(L) - 1;
    ObjWeakTableGet(L);
    if (clazz->size_constructors == 0) {
        //默认无参构造其实可以改成默认无构造函数自匹配，更人性化
        LuaObjUData *obj = makeObject(L, clazz, ObjLuaWeakTable, 2, 1 + nargs);
        return 1;
    } else {
        //遍历constructors
        LuaObjMethod *constructor = polymorphism_overload_method(L, NULL, 2, 1 + nargs, clazz, 1, 0, 0);
        if (constructor) {
            LuaObjAccessFlags flags = constructor->flags;
            if (flags & LUAOBJ_ACCESS_PUBLIC) {
            make_obj:;
                LuaObjUData *obj = makeObject(L, clazz, ObjLuaWeakTable, 2, 1 + nargs);
                lua_pushvalue(L, -1); //最终返回的值
                lua_pushnil(L);
                TValue *o = index2value(L, -1);
                setuvalue(L, o, constructor->udata);
                lua_pushcclosure(L, Objudata_MethodWrapCall, 2);
                for (int i = 1; i <= nargs; ++i) {
                    lua_pushvalue(L, 1 + i);
                }
                lua_call(L, nargs, 0);
                return 1;
            } else if (flags & LUAOBJ_ACCESS_PRIVATE) {
                int have_access = 0;
                CallInfo *lastCall = L->ci;
                if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到Lua函数层
                if (lastCall && lastCall->previous) lastCall = lastCall->previous; //来到MethodWrapCall层
                if (lastCall && ttypetag(s2v(lastCall->func.p)) == LUA_VCCL) {
                    CClosure *wrapcall = clCvalue(s2v(lastCall->func.p));
                    if ((Objudata_MethodWrapCall == wrapcall->f ||
                         Objudata_metaProxy == wrapcall->f)
                        && wrapcall->nupvalues >= 2) {
                        //检查是不是类内调用
                        LuaObjUData *classobj = (LuaObjUData *) getudatamem(uvalue(&wrapcall->upvalue[0]));
                        if (classobj == clazz || classobj->classholder == clazz->classholder)
                            have_access = 1;
                    }
                }
                if (!have_access) {
                    luaL_tolstring(L, 1, NULL);
                    luaG_runerror(L, "private constructor can only be called from '%s'", lua_tostring(L, -1));
                }
                goto make_obj;
            } else luaG_runerror(L, "constructor not have public or private access");
        } else luaG_runerror(L, "constructor not found");
    }
    return 0;
}

static int ObjudataMT__setup(lua_State *L, int idx) {
    lua_pushcfunction(L, ObjudataMT__tostring);
    lua_setfield(L, idx, "__tostring");
    lua_pushcfunction(L, ObjudataMT__index);
    lua_setfield(L, idx, "__index");
    lua_pushcfunction(L, ObjudataMT__newindex);
    lua_setfield(L, idx, "__newindex");
    lua_pushcfunction(L, ObjudataMT__call);
    lua_setfield(L, idx, "__call");
    return 0;
}

/*
 * 闭包方法调用
 * LuaObjUData界定private用，遇到这个方法定位:
 * 方法是否可以调用，在__call时判断，最近的Objudata_MethodWrapCall函数找上值，LuaObjUData就是它能否调用的核心认证方法（）
 * upval[1]:LuaObjUData
 * upval[2]:LuaObjMethod/LClosure
 * LClosure在定义阶段正确定义了上值，这里就一直拿着这个LClosure用，
 * 解释器内部执行时，方法函数会执行一个指令专门初始化self和super
 */
int Objudata_MethodWrapCall(lua_State *L) {
    int nargs = lua_gettop(L);
    LClosure *func = NULL;
    LuaObjUData *classOrObj = (LuaObjUData *) lua_touserdata(L, lua_upvalueindex(1));
    lua_pushvalue(L,lua_upvalueindex(2));
    TValue *o = index2value(L, -1);
    if (isLfunction(o)) {
        func = clLvalue(o);
    } else {
        LuaObjMethod *method = lua_touserdata(L, -1);
        func = method->func;
    }
    lua_pop(L, 1);
    //self/super需要预留好空间，因为寄存器初始分配因为包装接管了
    lua_pushnil(L), lua_insert(L, 1);
    lua_pushnil(L), lua_insert(L, 1);
    lua_pushnil(L);
    o = index2value(L, -1);
    setclLvalue(L, o, func);
    lua_insert(L, 1);
    // func [self] [super] arg1 arg2 ...
    lua_call(L, nargs + 2, LUA_MULTRET);
    return lua_gettop(L);
}


/*
 * arg1:LuaObjUData *clazz
 * arg2:LuaObjMethod *constructor
 * no-ret,no-class-verify,no-type-verify
 */
int Objudata_DefConstructor(lua_State *L) {
    LuaObjUData *clazz = lua_touserdata(L, 1); //R1
    LuaObjMethod *constructor = lua_touserdata(L, 2); //R2
    lua_settop(L, 2);
    //先拿到clazz的GC表
    lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R3 GC表
    int CLASS_GCIDX = luaL_len(L, -1); //R3
    //挂载到clazz的GC
    lua_pushvalue(L, -2); //R4 Constructor
    lua_rawseti(L, -2, ++CLASS_GCIDX); //R3
    lua_pop(L, 1); //R2
    //扩容赋值，然后把原来OBJLUA_UV_constructors内存更换
    LuaObjMethod **newconstructors = lua_newuserdatauv(L, sizeof(LuaObjMethod *) * (clazz->size_constructors + 1), 0);
    //R3
    memcpy(newconstructors, clazz->constructors, sizeof(LuaObjMethod *) * clazz->size_constructors);
    newconstructors[clazz->size_constructors++] = constructor;
    clazz->constructors = newconstructors;
    lua_setiuservalue(L, 1, OBJLUA_UV_constructors + 1); //R2
    return 0;
}

int Objudata_metaProxy(lua_State *L) {
    //uv0:LuaObjUData->classOrObj uv1:TString->metamethodname
    LuaObjUData *classOrObj = (LuaObjUData *) lua_touserdata(L, lua_upvalueindex(1));
    TString *metaname = tsvalue(index2value(L, lua_upvalueindex(2)));
    lua_remove(L, 1);
    int nargs = lua_gettop(L);
    LuaObjMethod *metamethod = polymorphism_overload_method(L, metaname, 1, nargs, classOrObj, 0, 1, 1);
    if (metamethod) {
        LClosure *func = metamethod->func;
        //self/super需要预留好空间，因为寄存器初始分配因为包装接管了
        lua_pushnil(L), lua_insert(L, 1);
        lua_pushnil(L), lua_insert(L, 1);
        lua_pushnil(L);
        TValue *o = index2value(L, -1);
        setclLvalue(L, o, func);
        lua_insert(L, 1);
        // func [self] [super] arg1 arg2 ...
        lua_call(L, nargs + 2, LUA_MULTRET);
        return lua_gettop(L);
    } else luaG_runerror(L, "metamethod '%s' not found", getstr(metaname));
    return 0;
}

/*
 * arg1:LuaObjUData *clazz
 * arg2:LuaObjMethod *metamethod
 * arg3:TString *metaname
 * no-ret,no-class-verify,no-type-verify
 */
int Objudata_DefMetaMethod(lua_State *L) {
    /*
     * metamethods比较特殊
     * 实际类/对象有预先设置__index、__newindex、__call，这都不允许改，更改之后整套系统就出错了。
     * 但是预先设置的__tostring允许改，毕竟无关紧要只是提供简要信息的。
     */
    LuaObjUData *clazz = lua_touserdata(L, 1); //R1
    LuaObjMethod *metamethod = lua_touserdata(L, 2); //R2
    lua_settop(L, 3); //R3
    if (lua_type(L, 3) != LUA_TSTRING)luaG_runerror(L, "anonymous metamethod not allowed"); //R3
    TString *metaname = tsvalue(index2value(L, 3)); //R3
    if (luaS_streq(metaname, luaS_newliteral(L, "__index")) ||
        luaS_streq(metaname, luaS_newliteral(L, "__newindex")) ||
        luaS_streq(metaname, luaS_newliteral(L, "__call"))) {
        luaG_runerror(L, "metamethod '%s' not allowed to redefine", getstr(metaname));
    }
    //先拿到clazz的GC表
    lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R4 GC表
    int CLASS_GCIDX = luaL_len(L, -1); //R4
    //挂载到clazz的GC
    lua_pushvalue(L, -3); //R5 MetaMethod
    lua_rawseti(L, -2, ++CLASS_GCIDX); //R4
    lua_pop(L, 1); //R3
    //扩容赋值，然后把原来OBJLUA_UV_metamethods内存更换
    LuaObjMethod **newmetamethods = lua_newuserdatauv(L, sizeof(LuaObjMethod *) * (clazz->size_metamethods + 1), 0);
    //R4
    memcpy(newmetamethods, clazz->metamethods, sizeof(LuaObjMethod *) * clazz->size_metamethods);
    newmetamethods[clazz->size_metamethods++] = metamethod;
    clazz->metamethods = newmetamethods;
    lua_setiuservalue(L, 1, OBJLUA_UV_metamethods + 1); //R3
    //还需要额外为其设置元表的代理（这时候不方便操作堆栈只能过来直接定义），直接覆盖就完事了，原内容失去引用就回收了
    lua_getmetatable(L, 1); //R4 classOrObj的元表
    Table *mt = hvalue(index2value(L,-1));
    lua_pushvalue(L, 3); //R6 metaname
    //uv0:LuaObjUData->classOrObj uv1:TString->metamethodname
    lua_pushvalue(L, 1); //R7 classOrObj
    lua_pushvalue(L, 3); //R8 metaname
    lua_pushcclosure(L, Objudata_metaProxy, 2); //R6
    lua_rawset(L, -3); //R4
    invalidateTMcache(mt); //强制刷新一次，以保证__gc以及其他元方法正确强制部署
    luaC_checkfinalizer(L, obj2gco(clazz->udata), mt);
    return 0;
}

/*
 * arg1:LuaObjUData *clazz
 * arg2:LuaObjField *field
 * arg3:Boolean deffield
 * no-ret,no-class-verify,no-type-verify
 */
int Objudata_DefField(lua_State *L) {
    lua_settop(L, 3);
    LuaObjUData *clazz = lua_touserdata(L, 1); //R3
    LuaObjField *field = lua_touserdata(L, 2); //R3
    int deffield = lua_toboolean(L, 3); //R3
    if (deffield) {
        //还有一种就是对象直接要过来类的静态字段
        //先拿到clazz的GC表
        lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R4 GC表
        int CLASS_GCIDX = luaL_len(L, -1); //R4
        //挂载到clazz的GC
        lua_pushvalue(L, -3); //R5 Field
        lua_rawseti(L, -2, ++CLASS_GCIDX); //R4
        lua_pop(L, 1); //R3
    }
    //fields扩容赋值，然后把原来OBJLUA_UV_fields内存更换
    LuaObjField **newfields = lua_newuserdatauv(L, sizeof(LuaObjField *) * (clazz->size_fields + 1), 0); //R4
    memcpy(newfields, clazz->fields, sizeof(LuaObjField *) * clazz->size_fields);
    newfields[clazz->size_fields++] = field;
    clazz->fields = newfields;
    lua_setiuservalue(L, 1, OBJLUA_UV_fields + 1); //R3
    return 0;
}

/*
 * arg1:LuaObjUData *clazz
 * arg2:LuaObjMethod *method
 * no-ret,no-class-verify,no-type-verify
 */
int Objudata_DefMethod(lua_State *L) {
    LuaObjUData *clazz = lua_touserdata(L, 1); //R1
    LuaObjMethod *method = lua_touserdata(L, 2); //R2
    lua_settop(L, 2);
    //先拿到clazz的GC表
    lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R3 GC表
    int CLASS_GCIDX = luaL_len(L, -1); //R3
    // printf("[%d]%d VS %d\n",__LINE__,CLASS_GCIDX,luaH_getn(hvalue(s2v(L->top.p-1))));
    //挂载到clazz的GC
    lua_pushvalue(L, -2); //R4 Method
    lua_rawseti(L, -2, ++CLASS_GCIDX); //R3
    lua_pop(L, 1); //R2
    //扩容赋值，然后把原来OBJLUA_UV_methods内存更换
    LuaObjMethod **newmethods = lua_newuserdatauv(L, sizeof(LuaObjMethod *) * (clazz->size_methods + 1), 0); //R3
    memcpy(newmethods, clazz->methods, sizeof(LuaObjMethod *) * clazz->size_methods);
    newmethods[clazz->size_methods++] = method;
    clazz->methods = newmethods;
    lua_setiuservalue(L, 1, OBJLUA_UV_methods + 1); //R2
    return 0;
}

/*
 * arg1:LuaObjUData *clazz
 * arg2:LuaObjMethod *method
 * no-ret,no-class-verify,no-type-verify
 */
LUAI_FUNC int Objudata_DefAbstractMethod(lua_State *L) {
    LuaObjUData *clazz = lua_touserdata(L, 1); //R1
    LuaObjMethod *method = lua_touserdata(L, 2); //R2
    lua_settop(L, 2);
    //先拿到clazz的GC表
    lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R3 GC表
    int CLASS_GCIDX = luaL_len(L, -1); //R3
    //挂载到clazz的GC
    lua_pushvalue(L, -2); //R4 Method
    lua_rawseti(L, -2, ++CLASS_GCIDX); //R3
    lua_pop(L, 1); //R2
    //扩容赋值，然后把原来OBJLUA_UV_abstractmethods内存更换
    LuaObjMethod **newabstractmethods = lua_newuserdatauv(L, sizeof(LuaObjMethod *) * (clazz->size_abstractmethods + 1),
                                                          0); //R3
    memcpy(newabstractmethods, clazz->abstractmethods, sizeof(LuaObjMethod *) * clazz->size_abstractmethods);
    newabstractmethods[clazz->size_abstractmethods++] = method;
    clazz->abstractmethods = newabstractmethods;
    lua_setiuservalue(L, 1, OBJLUA_UV_abstractmethods + 1); //R2
    return 0;
}

inline int luaS_streq(TString *left, TString *right) {
    if (left == right) return 1;
    else if (left == NULL || right == NULL) return 0;
    else if (left->tt != right->tt) return 0;
    else if (left->tt == LUA_VSHRSTR) return eqshrstr(left, right);
    else return luaS_eqlngstr(left, right);
}

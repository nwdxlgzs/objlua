#ifndef lobjudata_h
#define lobjudata_h

#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "lobject.h"
#include "lparser.h"
#include "lstate.h"

/*
重构垃圾回收，交回大部份GC控制权给Lua，
比如LuaObjField|LuaObjMethod等数据直接挂在LuaObjUData的元表里或者上值里直接保持引用（这里使用上值直接存，省的设计一个键了，毕竟lua5.4.7提供了，低版本把GC挂进元表里就OK了）
而不是暴力动GC针对标记的特殊USERDATA走特殊的GC实现
*/
// #define OBJLUA_GCHolder "<ObjLua:GCHolder>"
//相关东西都扔上值里，毕竟是Lua54
enum LuaObjUDataUVIndex {
    OBJLUA_UV_gc,
    OBJLUA_UV_fields,
    OBJLUA_UV_constructors,
    OBJLUA_UV_metamethods,
    OBJLUA_UV_methods,
    OBJLUA_UV_abstractmethods,
};

#define LuaObjUDataUpValueMinSize (OBJLUA_UV_abstractmethods + 1)
#define LuaObjFieldUpValueMinSize (OBJLUA_UV_fields + 1)
#define LuaObjMethodUpValueMinSize (OBJLUA_UV_gc + 1)
#define MethodArgTypeUpValueMinSize (OBJLUA_UV_gc + 1)
/*
破坏性修改G表结构存储并不友好，
应该遵循Lua设计风格放置在LUA_REGISTRYINDEX表下，
其中进行WeekTable化以解决局部类定义的回收问题，
防止收集了所有面向对象数据后局部表做不到释放。
*/
#define OBJLUA_WEAK_TABLE "__ObjLuaWeakTable"
typedef struct LuaObjUData LuaObjUData;

enum LuaObjAccessFlag {
    LUAOBJ_ACCESS_PUBLIC = 1 << 0,
    LUAOBJ_ACCESS_PRIVATE = 1 << 1,
    LUAOBJ_ACCESS_STATIC = 1 << 2,
    LUAOBJ_ACCESS_CONST = 1 << 3,
    LUAOBJ_ACCESS_META = 1 << 4,
    LUAOBJ_ACCESS_ABSTRACT = 1 << 5,
    LUAOBJ_ACCESS_CONSTRUCTOR = 1 << 6,
    LUAOBJ_ACCESS_NOWRAP = 1 << 7,
    LUAOBJ_ACCESS_ISMETHOD = 1 << 8,
    LUAOBJ_ACCESS_ISFIELD = 1 << 9,
};

typedef size_t LuaObjAccessFlags;

#define CommonFMHeader   LuaObjUData *self; LuaObjAccessFlags flags;TString *name
typedef struct FMStruct {
    CommonFMHeader;
} FMStruct;

typedef struct LuaObjField {
    CommonFMHeader;
    lu_byte initconst;
    //udata自己
    Udata *udata;
} LuaObjField;

enum BxTypeMask {
    TYPEMASK_is_vararg = 1 << 0, // 0001: 表示是否是可变参数
    TYPEMASK_is_typemode = 1 << 1, // 0010: 表示是否是类型模式
    TYPEMASK_is_classmode = 1 << 2, // 0100: 表示是否是类模式
};

typedef struct MethodArgType {
    lu_byte none;
    lu_byte is_vararg;
    lu_byte is_typemode; //pram:xxx
    lu_byte is_classmode; //param:<xxx>
    union {
        TString *type;
        LuaObjUData *clazz;
    };
    //udata自己
    Udata *udata;
} MethodArgType;

//语法解析阶段的MethodArgType
typedef struct llex_MethodArgType {
    lu_byte none;
    lu_byte is_vararg;
    lu_byte is_typemode; //pram:xxx
    lu_byte is_classmode; //param:<xxx>
    TString *type;
} llex_MethodArgType;

typedef struct llex_MethodArgTypes {
    lu_byte nargs;
    llex_MethodArgType *argtypes;
} llex_MethodArgTypes;

typedef struct LuaObjMethod {
    CommonFMHeader;
    LClosure *func; //很显然只能是Lua
    MethodArgType **argtypes;
    lu_byte nargs;
    //udata自己
    Udata *udata;
} LuaObjMethod;

struct LuaObjUData {
    TString *name;
    LuaObjUData *super; //如果是类，则指向父类，如果是对象实例，则指向父对象，顶级类/对象时为NULL
    LuaObjUData *classholder; //如果是类，则指向自己，如果是对象实例，则指向类
    lu_byte is_class; //是否是类
    //构造函数
    size_t size_constructors;
    LuaObjMethod **constructors;
    //元方法
    size_t size_metamethods;
    LuaObjMethod **metamethods;
    //字段
    size_t size_fields;
    LuaObjField **fields;
    //方法
    size_t size_methods;
    LuaObjMethod **methods;
    //抽象方法（要求继承的类必须完成的方法）
    size_t size_abstractmethods;
    LuaObjMethod **abstractmethods;
    //udata自己
    Udata *udata;
};
LUAI_FUNC int Objudata_init(lua_State *L);
LUAI_FUNC CClosure* RunAtPrepare(lua_State *L, const int nupvals, const lua_CFunction f);
LUAI_FUNC int RunAtOP_DEFCLASS(lua_State *L);
LUAI_FUNC int RunAtOP_DEFFIELD(lua_State *L);
LUAI_FUNC int RunAtOP_DEFMETHODARGTYPE(lua_State *L);
LUAI_FUNC int RunAtOP_DEFMETHOD(lua_State *L);
LUAI_FUNC int Objudata_MethodWrapCall(lua_State *L);

LUAI_FUNC int Objudata_metaProxy(lua_State *L);
LUAI_FUNC int Objudata_DefConstructor(lua_State *L);
LUAI_FUNC int Objudata_DefMetaMethod(lua_State *L);
LUAI_FUNC int Objudata_DefField(lua_State *L);
LUAI_FUNC int Objudata_DefMethod(lua_State *L);
LUAI_FUNC int Objudata_DefAbstractMethod(lua_State *L);

LUAI_FUNC inline int luaS_streq(TString *left, TString *right);

#endif

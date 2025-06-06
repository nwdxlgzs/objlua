#define lobjlualib_c
#define LUA_LIB
#include "lua.h"
#include "lualib.h"
#include "lapi.h"
#include "lauxlib.h"
#include "lobjudata.h"
#include "ltable.h"
#include "lvm.h"
#include "lstring.h"
#include "ldebug.h"

static inline TValue *getObjLuaWeakTable(lua_State *L) {
    TValue *ObjLuaWeakTable;
    if (!luaV_fastget(L, &G(L)->l_registry, luaS_newliteral(L, OBJLUA_WEAK_TABLE), ObjLuaWeakTable,
                      (TValue *) luaH_getshortstr))
        luaG_runerror(L, "'f_luaopen' not worked: <"OBJLUA_WEAK_TABLE"> is not initialized");
    return ObjLuaWeakTable;
}

LUA_API int objlua_getSuper(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    TValue *o = index2value(L, 1);
    const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
    const TValue *slot;
    if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) {
        lua_pushnil(L);
        return 1;
    }
    const LuaObjUData *classOrObj = lua_touserdata(L, 1);
    lua_pushnil(L);
    if (classOrObj->super) {
        o = index2value(L, -1);
        setuvalue(L, o, classOrObj->super->udata);
        return 1;
    }
    return 1;
}

LUA_API int objlua_getClass(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    TValue *o = index2value(L, 1);
    const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
    const TValue *slot;
    if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) {
        lua_pushnil(L);
        return 1;
    }
    const LuaObjUData *classOrObj = lua_touserdata(L, 1);
    lua_pushnil(L);
    o = index2value(L, -1);
    setuvalue(L, o, classOrObj->classholder->udata);
    return 1;
}

LUA_API int objlua_isClass(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    const TValue *o = index2value(L, 1);
    const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
    const TValue *slot;
    if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const LuaObjUData *classOrObj = lua_touserdata(L, 1);
    lua_pushboolean(L, classOrObj->is_class);
    return 1;
}

LUA_API int objlua_isObject(lua_State *L) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    const TValue *o = index2value(L, 1);
    const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
    const TValue *slot;
    if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const LuaObjUData *classOrObj = lua_touserdata(L, 1);
    lua_pushboolean(L, !classOrObj->is_class);
    return 1;
}

enum {
    FLAG_CommonGet_Fields,
    FLAG_CommonSet_Methods,
    FLAG_CommonGet_Constructors,
    FLAG_CommonGet_Metamethods,
    FLAG_CommonGet_AbstractMethods,
};

static int objlua_commonGet(lua_State *L, int flag, int declard) {
    luaL_checktype(L, 1, LUA_TUSERDATA);
    const TValue *o = index2value(L, 1);
    const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
    const TValue *slot;
    if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) {
        lua_newtable(L);
        return 1;
    }
    const LuaObjUData *classOrObj = lua_touserdata(L, 1);
    lua_newtable(L);
    int idx = 0;
    switch (flag) {
        case FLAG_CommonGet_Fields: {
        next_fields:
            for (int i = 0; i < classOrObj->size_fields; ++i) {
                LuaObjField *field = classOrObj->fields[i];
                lua_pushlightuserdata(L, field);
                lua_rawseti(L, -2, ++idx);
            }
            if (!declard && classOrObj->super) {
                classOrObj = classOrObj->super;
                goto next_fields;
            }
            break;
        }
        case FLAG_CommonSet_Methods: {
        next_methods:
            for (int i = 0; i < classOrObj->size_methods; ++i) {
                LuaObjMethod *method = classOrObj->methods[i];
                lua_pushlightuserdata(L, method);
                lua_rawseti(L, -2, ++idx);
            }
            if (!declard && classOrObj->super) {
                classOrObj = classOrObj->super;
                goto next_methods;
            }
            break;
        }
        case FLAG_CommonGet_Constructors: {
        next_constructors:
            for (int i = 0; i < classOrObj->size_constructors; ++i) {
                LuaObjMethod *constructor = classOrObj->constructors[i];
                lua_pushlightuserdata(L, constructor);
                lua_rawseti(L, -2, ++idx);
            }
            if (!declard && classOrObj->super) {
                classOrObj = classOrObj->super;
                goto next_constructors;
            }
            break;
        }
        case FLAG_CommonGet_Metamethods: {
        next_metamethods:
            for (int i = 0; i < classOrObj->size_metamethods; ++i) {
                LuaObjMethod *metamethod = classOrObj->metamethods[i];
                lua_pushlightuserdata(L, metamethod);
                lua_rawseti(L, -2, ++idx);
            }
            if (!declard && classOrObj->super) {
                classOrObj = classOrObj->super;
                goto next_metamethods;
            }
            break;
        }
        case FLAG_CommonGet_AbstractMethods: {
        next_abstractmethods:
            for (int i = 0; i < classOrObj->size_abstractmethods; ++i) {
                LuaObjMethod *abstractmethod = classOrObj->abstractmethods[i];
                lua_pushlightuserdata(L, abstractmethod);
                lua_rawseti(L, -2, ++idx);
            }
            if (!declard && classOrObj->super) {
                classOrObj = classOrObj->super;
                goto next_abstractmethods;
            }
        }
        default:
            break;
    }
    return 1;
}

LUA_API int objlua_getDeclaredFields(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Fields, 1);
}

LUA_API int objlua_getFields(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Fields, 0);
}

LUA_API int objlua_getDeclaredMethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonSet_Methods, 1);
}

LUA_API int objlua_getMethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonSet_Methods, 0);
}

LUA_API int objlua_getDeclaredConstructors(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Constructors, 1);
}

LUA_API int objlua_getConstructors(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Constructors, 0);
}

LUA_API int objlua_getDeclaredMetamethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Metamethods, 1);
}

LUA_API int objlua_getMetamethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_Metamethods, 0);
}

LUA_API int objlua_getDeclaredAbstractMethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_AbstractMethods, 1);
}

LUA_API int objlua_getAbstractMethods(lua_State *L) {
    return objlua_commonGet(L, FLAG_CommonGet_AbstractMethods, 0);
}

static int objlua_commonAccessFlag(lua_State *L, int flag_mask) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    const FMStruct *fms = lua_touserdata(L, 1); //Method/Field都有共同的头结构，内部也没定义逆天的int128之类的玩意，内存对齐正常
    lua_pushboolean(L, fms->flags & flag_mask);
    return 1;
}

LUA_API int objlua_isPublic(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_PUBLIC);
}

LUA_API int objlua_isPrivate(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_PRIVATE);
}

LUA_API int objlua_isStatic(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_STATIC);
}

LUA_API int objlua_isConst(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_CONST);
}

LUA_API int objlua_isMeta(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_META);
}

LUA_API int objlua_isAbstract(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_ABSTRACT);
}

LUA_API int objlua_isConstructor(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_CONSTRUCTOR);
}

LUA_API int objlua_isNoWrap(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_NOWRAP);
}

LUA_API int objlua_isMethod(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_ISMETHOD);
}

LUA_API int objlua_isField(lua_State *L) {
    return objlua_commonAccessFlag(L, LUAOBJ_ACCESS_ISFIELD);
}

LUA_API int objlua_getName(lua_State *L) {
    if (lua_type(L, 1) == LUA_TUSERDATA) {
        TValue *o = index2value(L, 1);
        const TValue *ObjLuaWeakTable = getObjLuaWeakTable(L);
        const TValue *slot;
        if (!luaV_fastget(L, ObjLuaWeakTable, o, slot, luaH_get) && ttistrue(slot)) goto badtype_ret;
        const LuaObjUData *classOrObj = lua_touserdata(L, 1);
        lua_pushnil(L);
        if (classOrObj->name) {
            o = index2value(L, -1);
            setsvalue(L, o, classOrObj->name);
        }
        return 1;
    } else if (lua_type(L, 1) == LUA_TLIGHTUSERDATA) {
        const FMStruct *fms = lua_touserdata(L, 1);
        lua_pushnil(L);
        if (fms->name) {
            TValue *o = index2value(L, -1);
            setsvalue(L, o, fms->name);
        }
        return 1;
    } else {
    badtype_ret:;
        lua_pushnil(L);
        return 1;
    }
}

LUA_API int objlua_getMethodFunction(lua_State *L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    const LuaObjMethod *method = lua_touserdata(L, 1);
    lua_pushnil(L);
    if (method->flags & LUAOBJ_ACCESS_ISMETHOD) {
        if (!(method->flags & LUAOBJ_ACCESS_ABSTRACT)) {
            TValue *o = index2value(L, -1);
            setclLvalue(L, o, method->func);
        }
    }
    return 1;
}

LUA_API int objlua_typeof(lua_State *L) {
    int b = lua_compare(L, 1, 2,LUA_OPTYPEOF);
    lua_pushboolean(L, b);
    return 1;
}

LUA_API int objlua_instanceof(lua_State *L) {
    int b = lua_compare(L, 1, 2,LUA_OPINSTANCEOF);
    lua_pushboolean(L, b);
    return 1;
}

/*
 * 热修复是一个文明的事情，这意味着生产阶段可以不重新更新启动即可切换为新方法
 * 参数1：需要换的方法（LUA_TLIGHTUSERDATA）
 * 参数2：替换函数（需要显式预留self、super参数）
 */
LUA_API int objlua_hotfixMethod(lua_State *L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    luaL_checktype(L, 2, LUA_TFUNCTION);
    lua_settop(L, 2);
    LuaObjMethod *method = lua_touserdata(L, 1);
    TValue *o = index2value(L, 2);
    if (method->flags & LUAOBJ_ACCESS_ISMETHOD && isLfunction(o)) {
        setuvalue(L, index2value(L, 1), method->udata);
        //先拿到GC挂载表挂载进去（原有的就不清除了，扔那里挂着，万一以后用到呢）
        lua_getiuservalue(L, 1, OBJLUA_UV_gc + 1); //R3
        int CLASS_GCIDX = luaL_len(L, -1);
        lua_pushvalue(L, 2); //R4
        lua_rawseti(L, -2, ++CLASS_GCIDX); //R3
        method->func = clLvalue(o);
        lua_pushboolean(L, 1);
        return 1;
    }
    lua_pushboolean(L, 0);
    return 1;
}

LUA_API int objlua_getMethodInit(lua_State *L) {
    CallInfo *lastCall = L->ci->previous; //lua
    lastCall = lastCall->previous; //wrap
    lua_settop(L, 0);
    lua_pushnil(L);
    lua_pushnil(L);
    if (lastCall && ttypetag(s2v(lastCall->func.p)) == LUA_VCCL) {
        CClosure *wrapcall = clCvalue(s2v(lastCall->func.p));
        if ((Objudata_MethodWrapCall == wrapcall->f ||
             Objudata_metaProxy == wrapcall->f)
            && wrapcall->nupvalues >= 2) {
            TValue *selfTV = &wrapcall->upvalue[0];
            LuaObjUData *classobj = (LuaObjUData *) getudatamem(uvalue(selfTV));
            setobj2n(L, index2value(L,1), selfTV);
            if (classobj->super) setuvalue(L, index2value(L, 2), classobj->super->udata);
        }
    }
    return 2;
}

LUA_API int objlua_getFieldValue(lua_State *L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    const LuaObjField *field = lua_touserdata(L, 1);
    lua_pushnil(L);
    if (field->flags & LUAOBJ_ACCESS_ISFIELD) {
        setobj2n(L, index2value(L, -1), &field->udata->uv[OBJLUA_UV_fields].uv);
    }
    return 1;
}

LUA_API int objlua_setFieldValue(lua_State *L) {
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    luaL_checkany(L, 2);
    const LuaObjField *field = lua_touserdata(L, 1);
    if (field->flags & LUAOBJ_ACCESS_ISFIELD) {
        setobj2n(L, &field->udata->uv[OBJLUA_UV_fields].uv, index2value(L, 2));
    }
    return 0;
}

LUA_API int objlua_getMethodArgTypes(lua_State *L) {
    lua_settop(L, 1);
    luaL_checktype(L, 1, LUA_TLIGHTUSERDATA);
    const LuaObjMethod *method = lua_touserdata(L, 1);
    lua_newtable(L);
    if (method->flags & LUAOBJ_ACCESS_ISMETHOD) {
        if (!(method->flags & LUAOBJ_ACCESS_ABSTRACT)) {
            lu_byte nargs = method->nargs;
            lua_pushinteger(L, nargs);
            lua_setfield(L, -2, "nargs");
            for (lu_byte i = 0; i < nargs; ++i) {
                MethodArgType* argType = method->argtypes[i];
                lua_newtable(L);
                lua_pushboolean(L, argType->none);
                lua_setfield(L, -2, "none");
                lua_pushboolean(L, argType->is_vararg);
                lua_setfield(L, -2, "is_vararg");
                lua_pushboolean(L, argType->is_typemode);
                lua_setfield(L, -2, "is_typemode");
                lua_pushboolean(L, argType->is_classmode);
                lua_setfield(L, -2, "is_classmode");
                lua_pushnil(L);
                if (argType->is_typemode) {
                    setsvalue(L, index2value(L, -1), argType->type);
                } else if (argType->is_classmode) {
                    setuvalue(L, index2value(L, -1), argType->clazz->udata);
                }
                lua_setfield(L, -2, "type");
                lua_rawseti(L, -2, i + 1);
            }
        }
    }
    return 1;
}

static const luaL_Reg objluaLib[] = {
    {"getSuper", objlua_getSuper},
    {"getClass", objlua_getClass},
    {"isClass", objlua_isClass},
    {"isObject", objlua_isObject},
    {"getDeclaredFields", objlua_getDeclaredFields},
    {"getFields", objlua_getFields},
    {"getDeclaredMethods", objlua_getDeclaredMethods},
    {"getMethods", objlua_getMethods},
    {"getDeclaredConstructors", objlua_getDeclaredConstructors},
    {"getConstructors", objlua_getConstructors},
    {"getDeclaredMetamethods", objlua_getDeclaredMetamethods},
    {"getMetamethods", objlua_getMetamethods},
    {"getDeclaredAbstractMethods", objlua_getDeclaredAbstractMethods},
    {"getAbstractMethods", objlua_getAbstractMethods},
    {"isPublic", objlua_isPublic},
    {"isPrivate", objlua_isPrivate},
    {"isStatic", objlua_isStatic},
    {"isConst", objlua_isConst},
    {"isMeta", objlua_isMeta},
    {"isAbstract", objlua_isAbstract},
    {"isConstructor", objlua_isConstructor},
    {"isNoWrap", objlua_isNoWrap},
    {"isMethod", objlua_isMethod},
    {"isField", objlua_isField},
    {"getName", objlua_getName},
    {"getMethodFunction", objlua_getMethodFunction},
    {"typeof", objlua_typeof},
    {"instanceof", objlua_instanceof},
    {"hotfixMethod", objlua_hotfixMethod},
    {"getMethodInit", objlua_getMethodInit},
    {"getFieldValue", objlua_getFieldValue},
    {"setFieldValue", objlua_setFieldValue},
    {"getMethodArgTypes", objlua_getMethodArgTypes},
    {NULL, NULL}
};

LUAMOD_API int luaopen_objlua(lua_State *L) {
    luaL_newlib(L, objluaLib);
    return 1;
}

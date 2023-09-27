#include <lua5.4/lua.h>
#include <lua5.4/lauxlib.h>
#include "lua-easy-curl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int new_readbuf(lua_State* L){
    int n;
    int type;
    size_t len;
    const char* data;

    void* buf;
    FILE* stream;

    n = lua_gettop(L);
    if(n != 1){
        return luaL_error(L, "This function takes 1 argument.");
    }
    type = lua_type(L,1);
    if(type != LUA_TSTRING){
        return luaL_error(L, "The first argument must be of type string.");
    }
    
    data = lua_tolstring(L,1,NULL);
    len = strlen(data);
    buf = malloc(len);
    memcpy(buf, data, len);
    stream = fmemopen(buf, len, "r");
    lua_pushlightuserdata(L, buf);
    lua_pushlightuserdata(L, stream);
    return 2;
}

static int close_readbuf(lua_State* L){
    int n;
    int type;
    void* buf;
    FILE* stream;

    n = lua_gettop(L);
    if(n != 2){
        return luaL_error(L, "This function takes two arguments.");
    }
    type = lua_type(L,2);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The second argument must be a FILE* stream.");
    }
    stream = lua_touserdata(L, 2);
    fclose(stream);

    type = lua_type(L,1);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The first argument must be a void* buffer.");
    }
    buf = lua_touserdata(L,1);
    free(buf);
    lua_pushnil(L);
    lua_pushnil(L);
    return 2;
}

static int new_writebuf(lua_State* L){
    int n;
    FILE* stream;
    void* buf;

    n = lua_gettop(L);
    if(n != 0){
        return luaL_error(L, "This function takes no arguments.");
    }
    buf = malloc(MAX_BUF_SIZE);
    stream = fmemopen(buf, MAX_BUF_SIZE, "w");
    if(stream == NULL){
        perror("fmemopen");
        return luaL_error(L, "fmemopen failed.");
    }
    lua_pushlightuserdata(L,buf);
    lua_pushlightuserdata(L,stream);
    return 2;
}

static int close_writebuf(lua_State* L){
    int n;
    int type;
    char* data;
    FILE* stream;

    n = lua_gettop(L);
    if(n != 2){
        return luaL_error(L, "This function takes 2 arguments.");
    }
    type = lua_type(L,1);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The second argument must be of type FILE* stream");
    }
    stream = lua_touserdata(L,2);
    fclose(stream);
    
    type = lua_type(L,2);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The first argument must be of type void* buf");
    }
    data = lua_touserdata(L,1);
    lua_pushstring(L, data);
    free(data);
    lua_pushnil(L);
    return 2;
}


static int easy_init(lua_State* L){
    CURL* hnd;
    hnd = curl_easy_init();
    lua_pushlightuserdata(L, hnd);
    return 1;
}

static int http_version(lua_State* L){
    int n;
    int type;
    const char* version_string;
    
    n = lua_gettop(L);
    if(n != 1){
        return luaL_error(L, "This function must take 1 argument.");
    }
    type = lua_type(L,1);
    if(type != LUA_TSTRING){
        return luaL_error(L, "The argument must be a string.");
    }
    version_string = lua_tolstring(L, 1, NULL);
    if(version_string == "1.0"){
        lua_pushinteger(L, CURL_HTTP_VERSION_1_0);
    } else if (version_string == "1.1"){
        lua_pushinteger(L, CURL_HTTP_VERSION_1_1);
    } else if (version_string == "2.0"){
        lua_pushinteger(L, CURL_HTTP_VERSION_2_0);
    } else if (version_string == "2TLS"){
        lua_pushinteger(L, CURL_HTTP_VERSION_2TLS);
    } else if (version_string == "2_PRIOR_KNOWLEDGE"){
        lua_pushinteger(L, CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE);
    } else if (version_string == "3.0"){
        lua_pushinteger(L, CURL_HTTP_VERSION_3);
    } else if (version_string == "3_ONLY"){
        lua_pushinteger(L, CURL_HTTP_VERSION_3ONLY);
    }
    return 1;
}

static int easy_setopt(lua_State* L){
    int n;
    int err;
    int type;
    const char* easy_option;
    FILE* stream;
    const struct curl_easyoption* opt;
    easy_opt_parameter_t p; 
    CURL* hnd;
    CURLcode ec;
    lua_Integer num;


    n = lua_gettop(L);
    if( n != 3){
        return luaL_error(L, "This function must take 3 arguments!");
    }
    type = lua_type(L, 1);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The first argument must be a CURL* handle");
    }
    hnd = lua_touserdata(L, 1);

    type = lua_type(L, 2);
    if(type != LUA_TSTRING){
        return luaL_error(L, "The second argument must be a CURL easy option string.");
    } 
    easy_option = lua_tolstring(L, 2, NULL);
    
    type = lua_type(L,3);
    if(type == LUA_TSTRING){
        p.param_str.param = lua_tolstring(L, 3, NULL);
        p.param_str.t = EASY_OPT_STRING_T;
    } else if (type == LUA_TNUMBER){
        p.param_long.param = lua_tointeger(L, 3);
        p.param_long.t = EASY_OPT_LONG_T;
    } else {
        err = strcmp(easy_option, "WRITEDATA");
        if(err == 0 && type == LUA_TLIGHTUSERDATA){ 
            stream = lua_touserdata(L, 3);
            p.param_long.param = (long)stream;
            p.param_long.t = EASY_OPT_LONG_T;
        } else {
            return luaL_error(L, "The third argument must be a CURL easy option parameter, either of type LONG, or of type STRING.");
        }
    }
    opt = curl_easy_option_by_name(easy_option); 
    if(opt){
        if(p.type.t == EASY_OPT_STRING_T){
            ec = curl_easy_setopt(hnd, (int)(opt->id), p.param_str.param);
        } else if(p.type.t == EASY_OPT_LONG_T){
            ec = curl_easy_setopt(hnd, (int)(opt->id), p.param_long.param);
        } else {
            return luaL_error(L, "No value option found.");
        }
    } else {
        return luaL_error(L, "The option string passed in is not a value CURL easy option parameter.");
    }
    return 0;
}

static int easy_perform(lua_State* L){
    CURLcode res;
    CURL* hnd;
    int n;
    int type;

    n = lua_gettop(L);
    if(n != 1){
        return luaL_error(L, "This function must take 1 argument.");
    }
    type = lua_type(L,1);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The function argument must be a CURL* easy handle.");
    }
    hnd = lua_touserdata(L, 1);
    res = curl_easy_perform(hnd);
    lua_pushinteger(L,res);
    return 1;
}

static int easy_cleanup(lua_State* L){
    CURL* hnd;
    int n;
    int type;
    n = lua_gettop(L);
    if(n != 1){
        return luaL_error(L, "This function must take 1 argument.");
    }
    type = lua_type(L, 1);
    if(type != LUA_TLIGHTUSERDATA){
        return luaL_error(L, "The function argument must be a CURL* easy handle.");
    }
    hnd = lua_touserdata(L,1);
    curl_easy_cleanup(hnd);
    lua_pushnil(L);
    return 1;
}


static const struct luaL_Reg easyCurl[] = {
    {"easy_cleanup", easy_cleanup},
    {"easy_perform", easy_perform},
    {"http_version", http_version},
    {"easy_setopt", easy_setopt},
    {"easy_init", easy_init},
    {"new_writebuf", new_writebuf},
    {"close_writebuf", close_writebuf}, 
    {"new_readbuf", new_readbuf},
    {"close_readbuf", close_readbuf},
    {NULL, NULL} 
};


int luaopen_easyCurl(lua_State* L){
    luaL_newlib(L, easyCurl);
    return 1;
}

#include <quickjs/quickjs.h>
#define COUNTOF(x) (sizeof(x) / sizeof((x)[0]))

static int 
fib(int n)
{
    if (n <= 0)
        return 0;
    else if (n == 1)
        return 1;
    else
        return fib(n - 1) + fib(n - 2);
}

static JSValue 
js_fib(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    int n, res;
    if (JS_ToInt32(ctx, &n, argv[0]))
        return JS_EXCEPTION;
    res = fib(n);
    return JS_NewInt32(ctx, res);
}

static const JSCFunctionListEntry js_fib_funcs[] = {
    JS_CFUNC_DEF("fib", 1, js_fib ),
};

static int 
js_fib_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, js_fib_funcs, COUNTOF(js_fib_funcs));
}

#ifdef JS_SHARED_LIBRARY
#warning js_shared_library
#define JS_INIT_MODULE js_init_module
#else
#warning not_js_shared_library
#define JS_INIT_MODULE js_init_module_fib
#endif

JSModuleDef *JS_INIT_MODULE(JSContext *ctx, const char *module_name)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, js_fib_init);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, js_fib_funcs, COUNTOF(js_fib_funcs));
    return m;
}

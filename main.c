#include <stdio.h>
#include <quickjs/quickjs-libc.h>
#include <gtk/gtk.h>

#define COUNTOF(x) (sizeof(x) / sizeof((x)[0]))

extern const uint8_t qjsc_index[];
extern const uint32_t qjsc_index_size;

extern const uint8_t qjsc_repl[];
extern const uint32_t qjsc_repl_size;

extern const uint8_t qjsc_mithril[];
extern const uint32_t qjsc_mithril_size;

static void
js_print(JSContext *ctx, int argc, JSValueConst *argv)
{
    size_t len;
    for (int i = 0; i < argc; i++) {
        if (i != 0) putchar(' ');
        const char *str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str) return;
        fwrite(str, 1, len, stdout);
        JS_FreeCString(ctx, str);
    }
}

static JSValue 
job_raf(JSContext *ctx, int argc, JSValueConst *argv)
{
    printf("executing job_raf\n");
    JSValue global = JS_GetGlobalObject(ctx);
    JSValue ret = JS_Call(ctx, argv[0], global, 0, NULL);
    printf("ret: ");
    js_print(ctx, 1, &ret);
    printf("\n");
    return JS_UNDEFINED;
}

static int
timeout_raf(void *ctx)
{
    printf("timeout_raf\n");
    js_std_loop((JSContext *) ctx);
    return 0; // dont repeat
}

static JSValue 
js_requestAnimationFrame(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    printf("raf called with %d args: ", argc);
    js_print(ctx, argc, argv);
    printf("\n");

    JS_EnqueueJob(ctx, job_raf, argc, argv);
    g_timeout_add(1, timeout_raf, ctx);

    return JS_UNDEFINED;
}

typedef struct DOMNode {
    GtkWidget *widget;
    const char *tag;
} DOMNode;

static JSClassID js_dom_class_id;

static JSClassDef js_dom_class = {
    "DOMNode",
}; 

static JSValue
js_dom_appendChild(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    printf("DomNode.appendChild called with %d args: ", argc);
    js_print(ctx, argc, argv);
    printf("\n");

    DOMNode *parent = JS_GetOpaque(this, js_dom_class_id);
    printf("parent: %p parent->widget: %p\n", parent, parent->widget);
    DOMNode *child = JS_GetOpaque(argv[0], js_dom_class_id);
    printf("child: %p child->widget: %p\n", child, child->widget);
    gtk_container_add(GTK_CONTAINER(parent->widget), child->widget);
    gtk_widget_show_all(child->widget);

    return JS_UNDEFINED;
}

typedef struct {
    JSContext *ctx;
    JSValue func;
    JSValue this;
} event_cb_ctx;

static void
clicked_cb(GtkButton *widget, void *p)
{
    event_cb_ctx *ectx = p;
    JSValue e = JS_NewObject(ectx->ctx);
    JS_SetPropertyStr(ectx->ctx, e, "type", JS_NewString(ectx->ctx, "click"));
    printf("calling clicked_cb\n");
    JSValue ret = JS_Call(ectx->ctx, ectx->func, ectx->this, 1, &e);
    printf("clicked_cb ret: ");
    js_print(ectx->ctx, 1, &ret);
    printf("\n");
}

static JSValue
js_dom_addEventListener(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    printf("DomNode.addEventListener called with %d args: ", argc);
    js_print(ctx, argc, argv);
    printf("\n");

    event_cb_ctx *ectx = calloc(1, sizeof(event_cb_ctx));
    ectx->ctx = ctx;

    if (JS_IsFunction(ctx, argv[1])) {
        printf("its a function\n");
        ectx->func = argv[1];
        ectx->this = this;
    } else {
        JSAtom k = JS_NewAtom(ctx, "handleEvent");
        if (JS_HasProperty(ctx, argv[1], k)) {
            printf("its a handler\n");
        } else {
            printf("what is it??\n");
        }
        ectx->func = JS_GetProperty(ctx, argv[1], k);
        // ectx->this = this;
        ectx->this = argv[1];
    }

    DOMNode *node = JS_GetOpaque(this, js_dom_class_id);
    const char *evt = JS_ToCString(ctx, argv[0]);
    if (!strcmp(evt, "click")) {
        g_signal_connect(node->widget, "clicked", G_CALLBACK(clicked_cb), ectx);
    } else {
        printf("ERR: unknown evt %s\n", evt);
    }

    return JS_UNDEFINED;
}

static JSValue 
js_dom_textContent_get(JSContext *ctx, JSValueConst this)
{
    DOMNode *node = JS_GetOpaque(this, js_dom_class_id);
    gchar *label;
    g_object_get(G_OBJECT(node->widget), "label", &label, NULL);
    return JS_NewString(ctx, label);
}

static JSValue 
js_dom_textContent_set(JSContext *ctx, JSValueConst this, JSValue val)
{
    DOMNode *node = JS_GetOpaque(this, js_dom_class_id);
    const char *label = JS_ToCString(ctx, val);
    printf("updating textContent to %s on widget %s %p\n", label, node->tag, node->widget);
    g_object_set(G_OBJECT(node->widget), "label", label, NULL);
    printf("set label\n");
    return JS_UNDEFINED;
}

static JSValue 
js_dom_firstChild_get(JSContext *ctx, JSValueConst this)
{
    DOMNode *node = JS_GetOpaque(this, js_dom_class_id);
    GList *children = gtk_container_get_children(GTK_CONTAINER(node->widget));
    GtkWidget *first_child = g_list_first(children)->data;
    JSObject *elobj = g_object_get_data(G_OBJECT(first_child), "jsval");
    if (elobj) {
        printf("already had associated jsval!\n");
        return JS_DupValue(ctx, JS_MKPTR(JS_TAG_OBJECT, elobj));
    } else {
        printf("no associated jsval\n");
        JSValue newel = JS_NewObjectClass(ctx, js_dom_class_id);
        DOMNode *newnode = calloc(1, sizeof(DOMNode));
        newnode->tag = "#";
        newnode->widget = first_child;
        JS_SetOpaque(newel, newnode);
        g_object_set_data(G_OBJECT(first_child), "jsval", JS_VALUE_GET_OBJ(JS_DupValue(ctx, newel)));
        return newel;
    }
}

static JSValue 
js_dom_firstChild_set(JSContext *ctx, JSValueConst this, JSValue val)
{
    // DOMNode *node = JS_GetOpaque(this, js_dom_class_id);
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_dom_proto_funcs[] = {
    JS_CFUNC_DEF("appendChild", 1, js_dom_appendChild),
    JS_CFUNC_DEF("addEventListener", 2, js_dom_addEventListener),
    JS_CGETSET_DEF("textContent", js_dom_textContent_get, js_dom_textContent_set),
    JS_CGETSET_DEF("nodeValue", js_dom_textContent_get, js_dom_textContent_set),
    JS_CGETSET_DEF("firstChild", js_dom_firstChild_get, js_dom_firstChild_set),
};

static JSValue
js_document_createTextNode(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    printf("createTextNode called with this: ");
    js_print(ctx, 1, &this);
    printf(" and %d args: ", argc);
    js_print(ctx, argc, argv);
    printf("\n");

    DOMNode *node = calloc(1, sizeof(DOMNode));
    node->tag = "#";
    node->widget = gtk_label_new(JS_ToCString(ctx, argv[0]));
    JSValue el = JS_NewObjectClass(ctx, js_dom_class_id);
    JS_SetOpaque(el, node);
    g_object_set_data(G_OBJECT(node->widget), "jsval", JS_VALUE_GET_OBJ(JS_DupValue(ctx, el)));
    return el;
}

static JSValue
js_document_createElement(JSContext *ctx, JSValueConst this, int argc, JSValueConst *argv)
{
    printf("createElement called with this: ");
    js_print(ctx, 1, &this);
    printf(" and %d args: ", argc);
    js_print(ctx, argc, argv);
    printf("\n");

    JSValue el = JS_NewObjectClass(ctx, js_dom_class_id);
    DOMNode *node = calloc(1, sizeof(DOMNode));

    const char *tag = JS_ToCString(ctx, argv[0]);
    if (!strcmp(tag, "vbox")) {
        node->widget = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        node->tag = "vbox";
    } else if (!strcmp(tag, "button")) {
        node->widget = gtk_button_new();
        node->tag = "button";
    } else {
        printf("ERR: unknown tag %s\n", tag);
    }
    JS_SetOpaque(el, node);
    g_object_set_data(G_OBJECT(node->widget), "jsval", JS_VALUE_GET_OBJ(JS_DupValue(ctx, el)));
    return el;
}

static void
add_dom_globals(JSContext *ctx, GtkWidget *root)
{
    JSValue global = JS_GetGlobalObject(ctx);

    JS_SetPropertyStr(ctx, global, "window", global);

    JSValue document = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, document, "createTextNode", 
        JS_NewCFunction(ctx, js_document_createTextNode, "createTextNode", 1));
    JS_SetPropertyStr(ctx, document, "createElement", 
        JS_NewCFunction(ctx, js_document_createElement, "createElement", 1));
    JS_SetPropertyStr(ctx, global, "document", document);

    JSValue body = JS_NewObjectClass(ctx, js_dom_class_id);
    DOMNode *node = calloc(1, sizeof(DOMNode));
    node->widget = root;
    node->tag = "body";
    JS_SetOpaque(body, node);
    g_object_set_data(G_OBJECT(root), "jsval", JS_VALUE_GET_OBJ(JS_DupValue(ctx, body)));
    JS_SetPropertyStr(ctx, document, "body", body);

    JS_SetPropertyStr(ctx, global, "requestAnimationFrame", 
        JS_NewCFunction(ctx, js_requestAnimationFrame, "requestAnimationFrame", 1));
}

static void
activate(GtkApplication *app, void *p)
{
    JSContext *ctx = p;
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "win0");

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(win), root);
    add_dom_globals(ctx, root);

    // init js engine
    js_std_eval_binary(ctx, qjsc_mithril, qjsc_mithril_size, 0);
    js_std_eval_binary(ctx, qjsc_index, qjsc_index_size, 0);
    // js_std_eval_binary(ctx, qjsc_repl, qjsc_repl_size, 0);
    js_std_loop(ctx);

    gtk_widget_show_all(win);
}

static void
gui(int argc, char *argv[], JSContext *ctx)
{
    GtkApplication *app = gtk_application_new("li.rje.qjs-exp", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), ctx);
    g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}

int
main(int argc, char *argv[])
{
    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();
    ctx = JS_NewContextRaw(rt);

    JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);
    JS_AddIntrinsicBaseObjects(ctx);
    JS_AddIntrinsicDate(ctx);
    JS_AddIntrinsicEval(ctx);
    JS_AddIntrinsicStringNormalize(ctx);
    JS_AddIntrinsicRegExp(ctx);
    JS_AddIntrinsicJSON(ctx);
    JS_AddIntrinsicProxy(ctx);
    JS_AddIntrinsicMapSet(ctx);
    JS_AddIntrinsicTypedArrays(ctx);
    JS_AddIntrinsicPromise(ctx);
    JS_AddIntrinsicBigInt(ctx);

    js_std_add_helpers(ctx, argc, argv);
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    // create dom class
    JS_NewClassID(&js_dom_class_id);
    JS_NewClass(rt, js_dom_class_id, &js_dom_class);

    // create dom proto
    JSValue dom_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, dom_proto, js_dom_proto_funcs, COUNTOF(js_dom_proto_funcs));
    JS_SetClassProto(ctx, js_dom_class_id, dom_proto);

    gui(argc, argv, ctx);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}

/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2013 The PHP Group                                |
  +----------------------------------------------------------------------+
  | http://www.opensource.org/licenses/mit-license.php  MIT License      |
  +----------------------------------------------------------------------+
  | Author: Stefan Siegl <stesie@brokenpipe.de>                          |
  +----------------------------------------------------------------------+
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

extern "C" {
#include "php.h"
#include "ext/date/php_date.h"
#include "ext/standard/php_string.h"
#include "zend_interfaces.h"
#include "zend_closures.h"
}

#include "php_v8js_macros.h"
#include "v8js_array_access.h"
#include "v8js_object_export.h"


static zval v8js_array_access_dispatch(zend_object *object, const char *method_name, int param_count,
									   uint32_t index, zval zvalue TSRMLS_DC) /* {{{ */
{
	zend_fcall_info fci;
	zval php_value;

	fci.size = sizeof(fci);
	fci.function_table = &object->ce->function_table;
	ZVAL_STRING(&fci.function_name, method_name);
	fci.symbol_table = NULL;
	fci.retval = &php_value;

	zval params[2];
	ZVAL_LONG(&params[0], index);
	params[1] = zvalue;

	fci.param_count = param_count;
	fci.params = params;

	fci.object = object;
	fci.no_separation = 0;

	zend_call_function(&fci, NULL TSRMLS_CC);
	zval_dtor(&fci.function_name);
	return php_value;
}
/* }}} */



void v8js_array_access_getter(uint32_t index, const v8::PropertyCallbackInfo<v8::Value>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	zval zvalue;
	ZVAL_UNDEF(&zvalue);

	zval php_value = v8js_array_access_dispatch(object, "offsetGet", 1, index, zvalue TSRMLS_CC);
	v8::Local<v8::Value> ret_value = zval_to_v8js(&php_value, isolate TSRMLS_CC);
	zval_dtor(&php_value);

	info.GetReturnValue().Set(ret_value);
}
/* }}} */

void v8js_array_access_setter(uint32_t index, v8::Local<v8::Value> value,
								  const v8::PropertyCallbackInfo<v8::Value>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	zval zvalue;
	ZVAL_UNDEF(&zvalue);

	if (v8js_to_zval(value, &zvalue, 0, isolate TSRMLS_CC) != SUCCESS) {
		info.GetReturnValue().Set(v8::Handle<v8::Value>());
		return;
	}

	zval php_value = v8js_array_access_dispatch(object, "offsetSet", 2, index, zvalue TSRMLS_CC);
	zval_dtor(&php_value);

	/* simply pass back the value to tell we intercepted the call
	 * as the offsetSet function returns void. */
	info.GetReturnValue().Set(value);

	/* if PHP wanted to hold on to this value, zend_call_function would
	 * have bumped the refcount. */
	zval_dtor(&zvalue);
}
/* }}} */


static int v8js_array_access_get_count_result(zend_object *object TSRMLS_DC) /* {{{ */
{
	zval zvalue;
	ZVAL_UNDEF(&zvalue);

	zval php_value = v8js_array_access_dispatch(object, "count", 0, 0, zvalue TSRMLS_CC);

	if(Z_TYPE(php_value) != IS_LONG) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Non-numeric return value from count() method");
		zval_dtor(&php_value);
		return 0;
	}

	int result = Z_LVAL(php_value);
	return result;
}
/* }}} */

static bool v8js_array_access_isset_p(zend_object *object, int index TSRMLS_DC) /* {{{ */
{
	zval zvalue;
	ZVAL_UNDEF(&zvalue);

	zval php_value = v8js_array_access_dispatch(object, "offsetExists", 1, index, zvalue TSRMLS_CC);

	if(Z_TYPE(php_value) != IS_TRUE && Z_TYPE(php_value) != IS_FALSE) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Non-boolean return value from offsetExists() method");
		zval_dtor(&php_value);
		return false;
	}

	return Z_TYPE(php_value) == IS_TRUE;
}
/* }}} */


static void v8js_array_access_length(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	int length = v8js_array_access_get_count_result(object TSRMLS_CC);
	info.GetReturnValue().Set(V8JS_INT(length));
}
/* }}} */

void v8js_array_access_deleter(uint32_t index, const v8::PropertyCallbackInfo<v8::Boolean>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	zval zvalue;
	ZVAL_UNDEF(&zvalue);

	zval php_value = v8js_array_access_dispatch(object, "offsetUnset", 1, index, zvalue TSRMLS_CC);
	zval_ptr_dtor(&php_value);

	info.GetReturnValue().Set(V8JS_BOOL(true));
}
/* }}} */

void v8js_array_access_query(uint32_t index, const v8::PropertyCallbackInfo<v8::Integer>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	/* If index is set, then return an integer encoding a v8::PropertyAttribute;
	 * otherwise we're expected to return an empty handle. */
	if(v8js_array_access_isset_p(object, index TSRMLS_CC)) {
		info.GetReturnValue().Set(V8JS_UINT(v8::PropertyAttribute::None));
	}
}
/* }}} */


void v8js_array_access_enumerator(const v8::PropertyCallbackInfo<v8::Array>& info) /* {{{ */
{
	v8::Isolate *isolate = info.GetIsolate();
	v8::Local<v8::Object> self = info.Holder();

	V8JS_TSRMLS_FETCH();

	v8::Local<v8::Value> php_object = self->GetHiddenValue(V8JS_SYM(PHPJS_OBJECT_KEY));
	zend_object *object = reinterpret_cast<zend_object *>(v8::External::Cast(*php_object)->Value());

	int length = v8js_array_access_get_count_result(object TSRMLS_CC);
	v8::Local<v8::Array> result = v8::Array::New(isolate, length);

	int i = 0;

	for(int j = 0; j < length; j ++) {
		if(v8js_array_access_isset_p(object, j TSRMLS_CC)) {
			result->Set(i ++, V8JS_INT(j));
		}
	}

	result->Set(V8JS_STR("length"), V8JS_INT(i));
	info.GetReturnValue().Set(result);
}
/* }}} */



void v8js_array_access_named_getter(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value> &info) /* {{{ */
{
	v8::String::Utf8Value cstr(property);
	const char *name = ToCString(cstr);

	if(strcmp(name, "length") == 0) {
		v8js_array_access_length(property, info);
		return;
	}

	v8::Local<v8::Value> ret_value = v8js_named_property_callback(property, info, V8JS_PROP_GETTER);

	if(ret_value.IsEmpty()) {
		v8::Isolate *isolate = info.GetIsolate();
		v8::Local<v8::Array> arr = v8::Array::New(isolate);
		v8::Local<v8::Value> prototype = arr->GetPrototype();

		if(!prototype->IsObject()) {
			/* ehh?  Array.prototype not an object? strange, stop. */
			info.GetReturnValue().Set(ret_value);
		}

		ret_value = prototype->ToObject()->Get(property);
	}

	info.GetReturnValue().Set(ret_value);
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

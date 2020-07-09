/*
   +----------------------------------------------------------------------+
   | Xdebug                                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2002-2020 Derick Rethans                               |
   +----------------------------------------------------------------------+
   | This source file is subject to version 1.01 of the Xdebug license,   |
   | that is bundled with this package in the file LICENSE, and is        |
   | available at through the world-wide-web at                           |
   | https://xdebug.org/license.php                                       |
   | If you did not receive a copy of the Xdebug license and are unable   |
   | to obtain it through the world-wide-web, please send a note to       |
   | derick@xdebug.org so we can mail you a copy immediately.             |
   +----------------------------------------------------------------------+
   | Authors: Derick Rethans <derick@xdebug.org>                          |
   +----------------------------------------------------------------------+
 */

#include "php.h"
#include "php_xdebug.h"

#include "develop_private.h"
#include "stack.h"

#include "lib/lib_private.h"
#include "lib/var_export_html.h"
#include "lib/var_export_line.h"
#include "lib/var_export_text.h"

ZEND_EXTERN_MODULE_GLOBALS(xdebug)

#define MODE_MUST_BE(m,n) \
	if (!XDEBUG_MODE_IS((m))) { \
		php_error(E_WARNING, "Function must be enabled in php.ini by setting 'xdebug.mode' to '%s'", (n)); \
		return; \
	}

/* {{{ proto void xdebug_var_dump(mixed var [, ...] )
   Outputs a fancy string representation of a variable */
PHP_FUNCTION(xdebug_var_dump)
{
	zval       *args;
	int         argc;
	int         i;
	xdebug_str *val;

	argc = ZEND_NUM_ARGS();

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (ZEND_NUM_ARGS() == 0 || zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	for (i = 0; i < argc; i++) {
		if (PG(html_errors)) {
			val = xdebug_get_zval_value_html(NULL, (zval*) &args[i], 0, NULL);
			PHPWRITE(val->d, val->l);
			xdebug_str_free(val);
		}
		else if ((XINI_DEV(cli_color) == 1 && xdebug_is_output_tty()) || (XINI_DEV(cli_color) == 2)) {
			val = xdebug_get_zval_value_ansi((zval*) &args[i], 0, NULL);
			PHPWRITE(val->d, val->l);
			xdebug_str_free(val);
		}
		else {
			val = xdebug_get_zval_value_text((zval*) &args[i], 0, NULL);
			PHPWRITE(val->d, val->l);
			xdebug_str_free(val);
		}
	}

	efree(args);
}
/* }}} */

/* {{{ proto void xdebug_debug_zval(mixed var [, ...] )
   Outputs a fancy string representation of a variable */
PHP_FUNCTION(xdebug_debug_zval)
{
	zval       *args;
	int         argc;
	int         i;
	xdebug_str *val;

	argc = ZEND_NUM_ARGS();

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (ZEND_NUM_ARGS() == 0 || zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	if (!(ZEND_CALL_INFO(EG(current_execute_data)->prev_execute_data) & ZEND_CALL_HAS_SYMBOL_TABLE)) {
		zend_rebuild_symbol_table();
	}

	for (i = 0; i < argc; i++) {
		if (Z_TYPE(args[i]) == IS_STRING) {
			zval debugzval;
			xdebug_str *tmp_name;

			xdebug_lib_set_active_symbol_table(EG(current_execute_data)->prev_execute_data->symbol_table);
			xdebug_lib_set_active_data(EG(current_execute_data)->prev_execute_data);

			tmp_name = xdebug_str_create(Z_STRVAL(args[i]), Z_STRLEN(args[i]));
			xdebug_get_php_symbol(&debugzval, tmp_name);
			xdebug_str_free(tmp_name);

			/* Reduce refcount for dumping */
			Z_TRY_DELREF(debugzval);

			php_printf("%s: ", Z_STRVAL(args[i]));
			if (Z_TYPE(debugzval) != IS_UNDEF) {
				if (PG(html_errors)) {
					val = xdebug_get_zval_value_html(NULL, &debugzval, 1, NULL);
					PHPWRITE(val->d, val->l);
				}
				else if ((XINI_DEV(cli_color) == 1 && xdebug_is_output_tty()) || (XINI_DEV(cli_color) == 2)) {
					val = xdebug_get_zval_value_ansi(&debugzval, 1, NULL);
					PHPWRITE(val->d, val->l);
				}
				else {
					val = xdebug_get_zval_value_line(&debugzval, 1, NULL);
					PHPWRITE(val->d, val->l);
				}
				xdfree(val);
				PHPWRITE("\n", 1);
			} else {
				PHPWRITE("no such symbol\n", 15);
			}

			/* Restore original refcount */
			Z_TRY_ADDREF(debugzval);
			zval_ptr_dtor_nogc(&debugzval);
		}
	}

	efree(args);
}
/* }}} */

/* {{{ proto void xdebug_debug_zval_stdout(mixed var [, ...] )
   Outputs a fancy string representation of a variable */
PHP_FUNCTION(xdebug_debug_zval_stdout)
{
	zval   *args;
	int     argc;
	int     i;

	argc = ZEND_NUM_ARGS();

	args = safe_emalloc(argc, sizeof(zval), 0);
	if (ZEND_NUM_ARGS() == 0 || zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	if (!(ZEND_CALL_INFO(EG(current_execute_data)->prev_execute_data) & ZEND_CALL_HAS_SYMBOL_TABLE)) {
		zend_rebuild_symbol_table();
	}

	for (i = 0; i < argc; i++) {
		if (Z_TYPE(args[i]) == IS_STRING) {
			zval        debugzval;
			xdebug_str *tmp_name;
			xdebug_str *val;

			xdebug_lib_set_active_symbol_table(EG(current_execute_data)->prev_execute_data->symbol_table);
			xdebug_lib_set_active_data(EG(current_execute_data)->prev_execute_data);

			tmp_name = xdebug_str_create(Z_STRVAL(args[i]), Z_STRLEN(args[i]));
			xdebug_get_php_symbol(&debugzval, tmp_name);
			xdebug_str_free(tmp_name);

			/* Reduce refcount for dumping */
			Z_TRY_DELREF(debugzval);

			printf("%s: ", Z_STRVAL(args[i]));
			if (Z_TYPE(debugzval) != IS_UNDEF) {
				val = xdebug_get_zval_value_line(&debugzval, 1, NULL);
				printf("%s(%zd)", val->d, val->l);
				xdebug_str_free(val);
				printf("\n");
			} else {
				printf("no such symbol\n\n");
			}

			/* Restore original refcount */
			Z_TRY_ADDREF(debugzval);
			zval_ptr_dtor_nogc(&debugzval);
		}
	}

	efree(args);
}
/* }}} */

PHP_FUNCTION(xdebug_start_error_collection)
{
	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (XG_LIB(do_collect_errors) == 1) {
		php_error(E_NOTICE, "Error collection was already started");
	}
	XG_LIB(do_collect_errors) = 1;
}

PHP_FUNCTION(xdebug_stop_error_collection)
{
	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (XG_LIB(do_collect_errors) == 0) {
		php_error(E_NOTICE, "Error collection was not started");
	}
	XG_LIB(do_collect_errors) = 0;
}

PHP_FUNCTION(xdebug_memory_usage)
{
	RETURN_LONG(zend_memory_usage(0));
}

PHP_FUNCTION(xdebug_peak_memory_usage)
{
	RETURN_LONG(zend_memory_peak_usage(0));
}

PHP_FUNCTION(xdebug_time_index)
{
	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	RETURN_DOUBLE(xdebug_get_utime() - XG_BASE(start_time));
}

/* {{{ proto void xdebug_print_function_stack([string message [, int options])
   Displays a stack trace */
PHP_FUNCTION(xdebug_print_function_stack)
{
	char *message = NULL;
	size_t message_len;
	function_stack_entry *i;
	char *tmp;
	zend_long options = 0;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|sl", &message, &message_len, &options) == FAILURE) {
		return;
	}

	i = xdebug_get_stack_frame(0);
	if (message) {
		tmp = xdebug_get_printable_stack(PG(html_errors), 0, message, ZSTR_VAL(i->filename), i->lineno, !(options & XDEBUG_STACK_NO_DESC));
	} else {
		tmp = xdebug_get_printable_stack(PG(html_errors), 0, "user triggered", ZSTR_VAL(i->filename), i->lineno, !(options & XDEBUG_STACK_NO_DESC));
	}
	php_printf("%s", tmp);
	xdfree(tmp);
}
/* }}} */

/* {{{ proto string xdebug_call_class()
   Returns the name of the calling class */
PHP_FUNCTION(xdebug_call_class)
{
	function_stack_entry *i;
	zend_long depth = 2;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &depth) == FAILURE) {
		return;
	}
	i = xdebug_get_stack_frame(depth);
	if (i) {
		if (i->function.class_name) {
			RETURN_STR(i->function.class_name);
		} else {
			RETURN_FALSE;
		}
	} else {
		return;
	}
}
/* }}} */

/* {{{ proto string xdebug_call_function()
   Returns the function name from which the current function was called from. */
PHP_FUNCTION(xdebug_call_function)
{
	function_stack_entry *i;
	zend_long depth = 2;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &depth) == FAILURE) {
		return;
	}
	i = xdebug_get_stack_frame(depth);
	if (i) {
		if (i->function.function) {
			RETURN_STRING(i->function.function);
		} else {
			RETURN_FALSE;
		}
	} else {
		return;
	}
}
/* }}} */

/* {{{ proto int xdebug_call_line()
   Returns the line number where the current function was called from. */
PHP_FUNCTION(xdebug_call_line)
{
	function_stack_entry *i;
	zend_long depth = 2;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &depth) == FAILURE) {
		return;
	}
	i = xdebug_get_stack_frame(depth);
	if (i) {
		RETURN_LONG(i->lineno);
	} else {
		return;
	}
}
/* }}} */

/* {{{ proto string xdebug_call_file()
   Returns the filename where the current function was called from. */
PHP_FUNCTION(xdebug_call_file)
{
	function_stack_entry *i;
	zend_long depth = 2;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &depth) == FAILURE) {
		return;
	}
	i = xdebug_get_stack_frame(depth);
	if (i) {
		RETURN_STR(i->filename);
	} else {
		return;
	}
}
/* }}} */

PHP_FUNCTION(xdebug_get_collected_errors)
{
	xdebug_llist_element *le;
	char                 *string;
	zend_bool             clear = 0;

	MODE_MUST_BE(XDEBUG_MODE_DEVELOP, "develop");

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "|b", &clear) == FAILURE) {
		return;
	}

	array_init(return_value);
	for (le = XDEBUG_LLIST_HEAD(XG_DEV(collected_errors)); le != NULL; le = XDEBUG_LLIST_NEXT(le))	{
		string = XDEBUG_LLIST_VALP(le);
		add_next_index_string(return_value, string);
	}

	if (clear) {
		xdebug_llist_destroy(XG_DEV(collected_errors), NULL);
		XG_DEV(collected_errors) = xdebug_llist_alloc(xdebug_llist_string_dtor);
	}
}

/* neptune extension for PHP */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <time.h>
#include <stdio.h>
#include "php.h"
#include "ext/standard/info.h"
#include "Zend/zend_compile.h"
#include "Zend/zend_types.h"
#include "php_neptune.h"
#include "hook.h"
#include "archive.h"


/* For compatibility with older PHP versions */
#ifndef ZEND_PARSE_PARAMETERS_NONE
#define ZEND_PARSE_PARAMETERS_NONE() \
	ZEND_PARSE_PARAMETERS_START(0, 0) \
	ZEND_PARSE_PARAMETERS_END()
#endif



/* {{{ void neptune_test1()
 */
PHP_FUNCTION(neptune_test1)
{
	ZEND_PARSE_PARAMETERS_NONE();

	php_printf("The extension %s is loaded and working!\r\n", "neptune");
}
/* }}} */

/* {{{ string neptune_test2( [ string $var ] )
 */
PHP_FUNCTION(neptune_test2)
{
	char *var = "World";
	size_t var_len = sizeof("World") - 1;
	zend_string *retval;

	ZEND_PARSE_PARAMETERS_START(0, 1)
		Z_PARAM_OPTIONAL
		Z_PARAM_STRING(var, var_len)
	ZEND_PARSE_PARAMETERS_END();

	retval = strpprintf(0, "Hello %s", var);

	RETURN_STR(retval);
}
/* }}}*/

/* {{{ arginfo
 */
ZEND_BEGIN_ARG_INFO(arginfo_neptune_test1, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO(arginfo_neptune_test2, 0)
	ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ neptune_functions[]
 */
static const zend_function_entry neptune_functions[] = {
	PHP_FE(neptune_test1,		arginfo_neptune_test1)
	PHP_FE(neptune_test2,		arginfo_neptune_test2)
	PHP_FE_END
};
/* }}} */

ZEND_DECLARE_MODULE_GLOBALS(neptune)

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("neptune.hook_name", "", PHP_INI_SYSTEM, OnUpdateStringUnempty, hook_name, zend_neptune_globals, neptune_globals)
PHP_INI_END()


PHP_GINIT_FUNCTION(neptune)
{
#if defined(COMPILE_DL_BCMATH) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    neptune_globals->hook_name = "";
}

// Module initialization
PHP_MINIT_FUNCTION(neptune)
{
    zend_function *target_func;
    zend_class_entry *target_ce;
    zend_function *target_method;

    REGISTER_INI_ENTRIES();

    neptune_hook_init();
	neptune_archive_init();

    return SUCCESS;
}

// Module shutdown
PHP_MSHUTDOWN_FUNCTION(neptune)
{
    // zend_function_override handles restoring the original function entry
    // when the module is unloaded, so no manual restore is needed here.
    return SUCCESS;
}

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(neptune)
{
#if defined(ZTS) && defined(COMPILE_DL_NEPTUNE)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif

	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(neptune)
{
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(neptune)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "neptune support", "enabled");
	php_info_print_table_end();
}
/* }}} */

/* {{{ neptune_module_entry
 */
zend_module_entry neptune_module_entry = {
	STANDARD_MODULE_HEADER,
	"neptune",					/* Extension name */
	neptune_functions,			/* zend_function_entry */
	PHP_MINIT(neptune),			/* PHP_MINIT - Module initialization */
	PHP_MSHUTDOWN(neptune),		/* PHP_MSHUTDOWN - Module shutdown */
	PHP_RINIT(neptune),			/* PHP_RINIT - Request initialization */
	PHP_RSHUTDOWN(neptune),		/* PHP_RSHUTDOWN - Request shutdown */
	PHP_MINFO(neptune),			/* PHP_MINFO - Module info */
	PHP_NEPTUNE_VERSION,		/* Version */
    PHP_MODULE_GLOBALS(neptune),
    PHP_GINIT(neptune),
    NULL,
    NULL,
	STANDARD_MODULE_PROPERTIES_EX
};
/* }}} */

#ifdef COMPILE_DL_NEPTUNE
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(neptune)
#endif

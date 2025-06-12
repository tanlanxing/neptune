
/* neptune extension for PHP */

#ifndef PHP_NEPTUNE_H
# define PHP_NEPTUNE_H

extern zend_module_entry neptune_module_entry;
# define phpext_neptune_ptr &neptune_module_entry

# define PHP_NEPTUNE_VERSION "0.1.0"

# if defined(ZTS) && defined(COMPILE_DL_NEPTUNE)
ZEND_TSRMLS_CACHE_EXTERN()
# endif

ZEND_BEGIN_MODULE_GLOBALS(neptune)
    char *hook_name;
ZEND_END_MODULE_GLOBALS(neptune)

ZEND_EXTERN_MODULE_GLOBALS(neptune)
#define NEPTUNE_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(neptune, v)

#endif	/* PHP_NEPTUNE_H */

#include <time.h>
#include <stdio.h>
#include "php.h"
#include "main/snprintf.h"
#include "Zend/zend_hash.h"
#include "ext/standard/info.h"
#include "Zend/zend_compile.h"
#include "php_neptune.h"
#include "hook.h"



static HashTable *original_handlers; // Store the original zend_function
static void my_handler(zend_execute_data *execute_data, zval *return_value);
static void log_call_stack(FILE *trace_file, zend_execute_data *execute_data);


// Function to log the call stack
static void log_call_stack(FILE *trace_file, zend_execute_data *execute_data) {
    zend_execute_data *frame = execute_data;
    int depth = 0;
    time_t now;
    char timestamp[20];

    time(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(trace_file, "--- Call Stack at %s ---\n", timestamp);

    while (frame) {
        const char *filename = NULL;
        uint32_t lineno = 0;
        const char *class_name = NULL;
        const char *function_name = NULL;

        // Get file and line from userland frames or script entry
        if (frame->func && ZEND_USER_CODE(frame->func->type)) {
             // Userland function or script
             filename = ZSTR_VAL(frame->func->op_array.filename);
             // In PHP 7, opline points to the current instruction.
             // The line number associated with the frame is often the line
             // of the call *in the previous frame*. However, getting that
             // precisely requires more opcode analysis. For simplicity and
             // typical stack trace format, we can use the line number of
             // the *next* instruction that would execute in the caller's frame,
             // or the current instruction's line in the current frame.
             // A common approach in internal traces is using the line from the opline.
             // Let's use frame->opline->lineno for the current frame's execution point.
             if (frame->opline) {
                 lineno = frame->opline->lineno;
             }

            // Get class and function name for userland code
            if (frame->func->common.scope) {
                 class_name = ZSTR_VAL(frame->func->common.scope->name);
            }
            if (frame->func->common.function_name) {
                 function_name = ZSTR_VAL(frame->func->common.function_name);
            }

        } else if (frame->func && frame->func->type == ZEND_INTERNAL_FUNCTION) {
            // Internal function
            if (frame->func->common.scope) {
                 class_name = ZSTR_VAL(frame->func->common.scope->name);
            }
            if (frame->func->common.function_name) {
                 function_name = ZSTR_VAL(frame->func->common.function_name);
            } else {
                 function_name = "[internal]"; // Fallback for unnamed internal funcs
            }
             // Internal functions typically don't have associated file/line in the same way
        // } else if (frame->op_array && frame->op_array->filename) {
        //     // Top-level script execution frame
        //     filename = ZSTR_VAL(frame->op_array->filename);
        //      if (frame->opline) {
        //          lineno = frame->opline->lineno;
        //      }
        //      function_name = "{main}"; // Represents the main script execution
        }


        // Format and print stack frame information
        fprintf(trace_file, "#%d ", depth);

        if (filename) {
            fprintf(trace_file, "%s(%u): ", filename, lineno);
        } else {
            fprintf(trace_file, "[internal function]: ");
        }

        if (class_name) {
             fprintf(trace_file, "%s::", class_name);
        }

        if (function_name) {
             fprintf(trace_file, "%s()", function_name);
        } else {
             fprintf(trace_file, "{unknown_code}"); // Should not happen often with checks above
        }

        fprintf(trace_file, "\n");

        depth++;
        frame = frame->prev_execute_data; // Move to the previous frame in the stack
    }

    fprintf(trace_file, "-----------------------\n\n");
}


// Our custom handler for mysqli_stmt_bind_result
// The signature is the same as internal function handlers
static void my_handler(zend_execute_data *execute_data, zval *return_value)
{
    zif_handler handler;
    FILE *trace_file = NULL;
    zend_execute_data *pre = execute_data->prev_execute_data;
    const char *class_name = NULL;
    char function_name[1024];

    // Open the trace file for appending
    char *trace_file_name = "/tmp/neptune.log";
    trace_file = fopen(trace_file_name, "a");

    if (trace_file) {
        // Log the complete call stack
        // Start tracing from the frame *before* this handler's frame,
        // as this handler itself is part of the stack.
        // The execute_data passed here is for my_mysqli_stmt_bind_result_handler.
        // We want the stack trace of the PHP code that called mysqli_stmt_bind_result.
        // The caller frame of *this handler* is the frame of the original
        // mysqli_stmt_bind_result internal function call.
        // The frame *before that* is the PHP code that made the call.
        // So we start from execute_data->prev_execute_data->prev_execute_data.
        // However, a simpler and often sufficient approach for a backtrace
        // is to just start from the frame of the *original* internal function call,
        // which is the frame immediately preceding this handler's frame.
        // Let's start from execute_data->prev_execute_data.
        log_call_stack(trace_file, pre);

        fclose(trace_file);
    } else {
        // Log an error if the file cannot be opened (this would typically go to PHP's error log)
        php_error_docref(NULL, E_WARNING, "Could not open /tmp/caller.trace for writing");
    }

    // Call the original mysqli_stmt_bind_result handler
    // We call the handler directly from the stored original function struct.
    if (execute_data->func->common.scope) {
        class_name = ZSTR_VAL(execute_data->func->common.scope->name);
    }
    if (execute_data->func->common.function_name) {
        if (class_name) {
            snprintf(function_name, 1024, "%s::%s", class_name, ZSTR_VAL(execute_data->func->common.function_name));
        } else {
            strcpy(function_name, ZSTR_VAL(execute_data->func->common.function_name));
        }
    }
    if (strlen(function_name) > 0) {
        zend_string *zfname = zend_string_init(function_name, strlen(function_name), 1);
        handler = zend_hash_find_ptr(original_handlers, zfname);
        if (handler) {
            handler(execute_data, return_value);
        } else {
            php_error_docref(NULL, E_WARNING, "Original handler not found during execution!");
            // Consider setting an exception or returning an error here if the original
            // function call is critical.
        }
        zend_string_release(zfname);
    } else {

    }
}

void hook_function(char *function_name)
{
    zend_function *target_func;
    zend_string *fname = zend_string_init(function_name, strlen(function_name), 1);

    // Find the original mysqli_stmt_bind_result function entry
    if ((target_func = zend_hash_find_ptr(CG(function_table), fname)) != NULL) {

        if (target_func->type == ZEND_INTERNAL_FUNCTION) {
            zend_hash_add_ptr(original_handlers, fname,
                target_func->internal_function.handler);
            // Override the function handler
            target_func->internal_function.handler = my_handler;
            // php_printf("Hooked %s successfully.\n", function_name); // For debugging loading
        } else {
            // This should not happen for mysqli_stmt_bind_result, but good practice
            php_error_docref(NULL, E_WARNING, "%s is not an internal function", function_name);
        }
    } else {
        php_error_docref(NULL, E_WARNING, "Could not find %s function entry. Is mysqli loaded?", function_name);
    }

    zend_string_release(fname); // Release the zend_string for the function name
}

void hook_method(char *method_name)
{
    const char *delim = "::";
    char *class_name, *function_name, *token, *str_dup, *saveptr;
    str_dup = estrndup(method_name, strlen(method_name));
    class_name = php_strtok_r(str_dup, delim, &saveptr);
    function_name = php_strtok_r(NULL, delim, &saveptr);

    zend_class_entry *target_ce;
    zend_function *target_method;
    zend_string *z_method_name = zend_string_init(method_name, strlen(method_name), 1);
    zend_string *cname = zend_string_init(class_name, strlen(class_name), 1);
    zend_string *mname = zend_string_init(function_name, strlen(function_name), 1);

    // Find the mysqli_stmt class entry
    if ((target_ce = zend_hash_find_ptr(CG(class_table), cname)) != NULL) {
        // Find the bind_result method within the class's function table
        if ((target_method = zend_hash_find_ptr(&target_ce->function_table, mname)) != NULL) {
            // Override the function handler
            // mysqli_stmt::bind_result original handler is zif_mysqli_stmt_bind_result
            zend_hash_add_ptr(original_handlers, z_method_name,
                target_method->internal_function.handler);
            target_method->internal_function.handler = my_handler;
        } else {
            php_error_docref(NULL, E_WARNING, "Could not find mysqli_stmt::bind_result method entry");
        }
    } else {
        php_error_docref(NULL, E_WARNING, "Could not find mysqli_stmt class entry. Is mysqli loaded?");
    }
    efree(str_dup);
    zend_string_release(z_method_name);
    zend_string_release(cname); // Release the zend_string for the class name
    zend_string_release(mname); // Release the zend_string for the method name
}

void neptune_hook_init(void)
{
    char *hook_name = NULL;
    const char *delim = ",";
    const char *delim1 = "::";
    char *found = NULL;
    char *token, *str_dup, *saveptr;

    hook_name = NEPTUNE_G(hook_name);
    // php_printf("directive hook_name is %s .\n", ZSTR_VAL(hook_name));
    if (strlen(hook_name) <= 0) {
        return;
    }

    original_handlers = pemalloc(sizeof(HashTable), 1);
    zend_hash_init(original_handlers, 0, NULL, ZVAL_PTR_DTOR, 1);

    str_dup = estrndup(hook_name, strlen(hook_name));
    token = php_strtok_r(str_dup, delim, &saveptr);

    while (token) {
        // 处理每个token
        php_printf("Token: %s\n", token);
        found = (char*)php_memnstr(token, delim1, strlen(delim1), token + strlen(token));
        if (found) {
            hook_method(token);
        } else {
            hook_function(token);
        }

        token = php_strtok_r(NULL, delim, &saveptr);
    }

    efree(str_dup);
}
/*
   +----------------------------------------------------------------------+
   | Elastic APM agent for PHP                                            |
   +----------------------------------------------------------------------+
   | Copyright (c) 2020 Elasticsearch B.V.                                |
   +----------------------------------------------------------------------+
   | Elasticsearch B.V. licenses this file under the Apache 2.0 License.  |
   | See the LICENSE file in the project root for more information.       |
   +----------------------------------------------------------------------+
 */

#include "util_for_PHP.h"
#include <php_main.h>
#include "elastic_apm_version.h"
#if defined(PHP_WIN32) && ! defined(CURL_STATICLIB)
#   define CURL_STATICLIB
#endif
#include <curl/curl.h>
#include "util.h"
#include "time_util.h"
#include "ConfigManager.h"

#define ELASTIC_APM_CURRENT_LOG_CATEGORY ELASTIC_APM_LOG_CATEGORY_UTIL

ResultCode loadPhpFile( const char* filename TSRMLS_DC )
{
    ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG( "filename: `%s'", filename );

    ResultCode resultCode;
    size_t filename_len = strlen( filename );
    zval dummy;
    zend_file_handle file_handle;
    zend_op_array * new_op_array = NULL;
    zval result;
    int ret;
    zend_string* opened_path = NULL;

    if ( filename_len == 0 )
    {
        ELASTIC_APM_LOG_ERROR( "filename_len == 0" );
        resultCode = resultFailure;
        goto failure;
    }

    ret = php_stream_open_for_zend_ex( filename, &file_handle, USE_PATH | STREAM_OPEN_FOR_INCLUDE );
    if ( ret != SUCCESS )
    {
        ELASTIC_APM_LOG_ERROR( "php_stream_open_for_zend_ex failed. Return value: %d. filename: %s", ret, filename );
        resultCode = resultFailure;
        goto failure;
    }

    if ( ! file_handle.opened_path )
    {
        file_handle.opened_path = zend_string_init( filename, filename_len, 0 );
    }
    opened_path = zend_string_copy( file_handle.opened_path );
    ZVAL_NULL( &dummy );
    if ( ! zend_hash_add( &EG( included_files ), opened_path, &dummy ) )
    {
        ELASTIC_APM_LOG_ERROR( "zend_hash_add failed. filename: %s", filename );
        zend_file_handle_dtor( &file_handle );
        resultCode = resultFailure;
        goto failure;
    }

    new_op_array = zend_compile_file( &file_handle, ZEND_REQUIRE );
    zend_destroy_file_handle( &file_handle );
    if ( ! new_op_array )
    {
        ELASTIC_APM_LOG_ERROR( "zend_compile_file failed. filename: %s", filename );
        resultCode = resultFailure;
        goto failure;
    }

    ZVAL_UNDEF( &result );
    zend_execute( new_op_array, &result );
    zval_ptr_dtor( &result );

    resultCode = resultSuccess;

    finally:
    if ( new_op_array )
    {
        destroy_op_array( new_op_array );
        efree( new_op_array );
        new_op_array = NULL;
    }

    if ( opened_path )
    {
        zend_string_release( opened_path );
        opened_path = NULL;
    }

    ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
    return resultCode;

    failure:
    goto finally;
}

void getArgsFromZendExecuteData( zend_execute_data* execute_data, size_t dstArraySize, zval dstArray[], uint32_t* argsCount )
{
    *argsCount = ZEND_CALL_NUM_ARGS( execute_data );
    ZEND_PARSE_PARAMETERS_START( /* min_num_args: */ 0, /* max_num_args: */ ( (int) dstArraySize ) )
    Z_PARAM_OPTIONAL
    ELASTIC_APM_FOR_EACH_INDEX( i, *argsCount )
    {
        zval* pArgAsZval;
        Z_PARAM_ZVAL( pArgAsZval )
        dstArray[ i ] = *pArgAsZval;
    }
    ZEND_PARSE_PARAMETERS_END();
}

typedef void (* ConsumeZvalFunc)( void* ctx, const zval* pZval );

static
ResultCode callPhpFunction( StringView phpFunctionName, LogLevel logLevel, uint32_t argsCount, zval args[], ConsumeZvalFunc consumeRetVal, void* consumeRetValCtx )
{
    ELASTIC_APM_LOG_FUNCTION_ENTRY_MSG_WITH_LEVEL( logLevel, "phpFunctionName: `%.*s', argsCount: %u"
                                                  , (int) phpFunctionName.length, phpFunctionName.begin, argsCount );

    ResultCode resultCode;
    zval phpFunctionNameAsZval;
    ZVAL_UNDEF( &phpFunctionNameAsZval );
    zval phpFunctionRetVal;
    ZVAL_UNDEF( &phpFunctionRetVal );

    ZVAL_STRINGL( &phpFunctionNameAsZval, phpFunctionName.begin, phpFunctionName.length );
    // call_user_function(function_table, object, function_name, retval_ptr, param_count, params)
    int callUserFunctionRetVal = call_user_function(
            EG( function_table )
            , /* object: */ NULL
            , /* function_name: */ &phpFunctionNameAsZval
            , /* retval_ptr: */ &phpFunctionRetVal
            , argsCount
            , args TSRMLS_CC );
    if ( callUserFunctionRetVal != SUCCESS )
    {
        ELASTIC_APM_LOG_ERROR( "call_user_function failed. Return value: %d. PHP function name: `%.*s'. argsCount: %u."
                , callUserFunctionRetVal, (int) phpFunctionName.length, phpFunctionName.begin, argsCount );
        resultCode = resultFailure;
        goto failure;
    }

    if ( consumeRetVal != NULL ) consumeRetVal( consumeRetValCtx, &phpFunctionRetVal );

    resultCode = resultSuccess;

    finally:
    zval_dtor( &phpFunctionNameAsZval );
    zval_dtor( &phpFunctionRetVal );

    ELASTIC_APM_LOG_FUNCTION_EXIT_MSG_WITH_LEVEL( logLevel, "resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
    return resultCode;

    failure:
    goto finally;
}

static
void consumeBoolRetVal( void* ctx, const zval* pZval )
{
    ELASTIC_APM_ASSERT_VALID_PTR( ctx );
    ELASTIC_APM_ASSERT_VALID_PTR( pZval );

    if ( Z_TYPE_P( pZval ) == IS_TRUE )
    {
        *((bool*)ctx) = true;
    }
    else
    {
        ELASTIC_APM_ASSERT( Z_TYPE_P( pZval ) == IS_FALSE, "Z_TYPE_P( pZval ) as int: %d", (int) ( Z_TYPE_P( pZval ) ) );
        *((bool*)ctx) = false;
    }
}

ResultCode callPhpFunctionRetBool( StringView phpFunctionName, LogLevel logLevel, uint32_t argsCount, zval args[], bool* retVal )
{
    return callPhpFunction( phpFunctionName, logLevel, argsCount, args, consumeBoolRetVal, /* consumeRetValCtx: */ retVal );
}

ResultCode callPhpFunctionRetVoid( StringView phpFunctionName, LogLevel logLevel, uint32_t argsCount, zval args[] )
{
    return callPhpFunction( phpFunctionName, logLevel, argsCount, args, /* consumeRetValCtx: */ NULL, /* consumeRetValCtx: */ NULL );
}

static
void consumeZvalRetVal( void* ctx, const zval* pZval )
{
    ELASTIC_APM_ASSERT_VALID_PTR( ctx );
    ELASTIC_APM_ASSERT_VALID_PTR( pZval );

    ZVAL_COPY( ((zval*)ctx), pZval );
}

ResultCode callPhpFunctionRetZval( StringView phpFunctionName, LogLevel logLevel, uint32_t argsCount, zval args[], zval* retVal )
{
    return callPhpFunction( phpFunctionName, logLevel, argsCount, args, consumeZvalRetVal, retVal );
}

// Log response
static
size_t logResponse( void* data, size_t unusedSizeParam, size_t dataSize, void* unusedUserDataParam )
{
    // https://curl.haxx.se/libcurl/c/CURLOPT_WRITEFUNCTION.html
    // size (unusedSizeParam) is always 1
    ELASTIC_APM_UNUSED( unusedSizeParam );
    ELASTIC_APM_UNUSED( unusedUserDataParam );

    ELASTIC_APM_LOG_DEBUG( "APM Server's response body [length: %"PRIu64"]: %.*s", (UInt64) dataSize, (int) dataSize, (const char*) data );
    return dataSize;
}

ResultCode sendEventsToApmServer( const ConfigSnapshot* config, StringView serializedEvents )
{
    ELASTIC_APM_LOG_DEBUG_FUNCTION_ENTRY_MSG(
            "Sending events to APM Server..."
            " serializedEvents [length: %"PRIu64"]:\n%.*s"
            , (UInt64) serializedEvents.length, (int) serializedEvents.length, serializedEvents.begin );

    ResultCode resultCode;
    CURL* curl = NULL;
    CURLcode result;
    enum
    {
        authBufferSize = 256
    };
    char auth[authBufferSize];
    enum
    {
        userAgentBufferSize = 100
    };
    char userAgent[userAgentBufferSize];
    enum
    {
        urlBufferSize = 256
    };
    char url[urlBufferSize];
    struct curl_slist* chunk = NULL;
    int snprintfRetVal;

    /* get a curl handle */
    curl = curl_easy_init();
    if ( curl == NULL )
    {
        ELASTIC_APM_LOG_ERROR( "curl_easy_init() returned NULL" );
        resultCode = resultFailure;
        goto failure;
    }

    curl_easy_setopt( curl, CURLOPT_POST, 1L );
    curl_easy_setopt( curl, CURLOPT_POSTFIELDS, serializedEvents.begin );
    curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, logResponse );
    curl_easy_setopt( curl, CURLOPT_CONNECTTIMEOUT_MS, (long) durationToMilliseconds( config->serverConnectTimeout ) );

    // Authorization with secret token if present
    if ( ! isNullOrEmtpyString( config->secretToken ) )
    {
        snprintfRetVal = snprintf( auth, authBufferSize, "Authorization: Bearer %s", config->secretToken );
        if ( snprintfRetVal < 0 || snprintfRetVal >= authBufferSize )
        {
            ELASTIC_APM_LOG_ERROR( "Failed to build Authorization header. snprintfRetVal: %d", snprintfRetVal );
            resultCode = resultFailure;
            goto failure;
        }
        chunk = curl_slist_append( chunk, auth );
    }
    chunk = curl_slist_append( chunk, "Content-Type: application/x-ndjson" );
    curl_easy_setopt( curl, CURLOPT_HTTPHEADER, chunk );

    // User agent - "elasticapm-<language>/<version>" (no separators between "elastic" and "apm" is on purpose)
	// For example, "elasticapm-java/1.2.3" or "elasticapm-dotnet/1.2.3"
    snprintfRetVal = snprintf( userAgent, userAgentBufferSize, "elasticapm-php/%s", PHP_ELASTIC_APM_VERSION );
    if ( snprintfRetVal < 0 || snprintfRetVal >= authBufferSize )
    {
        ELASTIC_APM_LOG_ERROR( "Failed to build User-Agent header. snprintfRetVal: %d", snprintfRetVal );
        resultCode = resultFailure;
        goto failure;
    }
    curl_easy_setopt( curl, CURLOPT_USERAGENT, userAgent );

    snprintfRetVal = snprintf( url, urlBufferSize, "%s/intake/v2/events", config->serverUrl );
    if ( snprintfRetVal < 0 || snprintfRetVal >= authBufferSize )
    {
        ELASTIC_APM_LOG_ERROR( "Failed to build full URL to APM Server's intake API. snprintfRetVal: %d", snprintfRetVal );
        resultCode = resultFailure;
        goto failure;
    }
    curl_easy_setopt( curl, CURLOPT_URL, url );

    result = curl_easy_perform( curl );
    if ( result != CURLE_OK )
    {
        ELASTIC_APM_LOG_ERROR( "Sending events to APM Server failed. URL: `%s'. Error message: `%s'.", url, curl_easy_strerror( result ) );
        resultCode = resultFailure;
        goto failure;
    }

    long responseCode;
    curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &responseCode );
    ELASTIC_APM_LOG_DEBUG( "Sent events to APM Server. Response HTTP code: %ld. URL: `%s'.", responseCode, url );

    resultCode = resultSuccess;

    finally:
    if ( curl != NULL ) curl_easy_cleanup( curl );

    ELASTIC_APM_LOG_DEBUG_FUNCTION_EXIT_MSG( "resultCode: %s (%d)", resultCodeToString( resultCode ), resultCode );
    return resultCode;

    failure:
    goto finally;
}

/**
 * @file ftview_common.c
 * @brief Common utilities for mini-factorytalk-view-hmi
 */

#include "ftview_common.h"

const char *ftview_strerror(ftview_err_t err)
{
    switch (err) {
    case FTVIEW_OK:                  return "Success";
    case FTVIEW_ERR_NULL_PTR:        return "Null pointer";
    case FTVIEW_ERR_OUT_OF_MEMORY:   return "Out of memory";
    case FTVIEW_ERR_TAG_NOT_FOUND:   return "Tag not found";
    case FTVIEW_ERR_DUPLICATE_TAG:   return "Duplicate tag name";
    case FTVIEW_ERR_INVALID_PARAM:   return "Invalid parameter";
    case FTVIEW_ERR_BUFFER_FULL:     return "Buffer full";
    case FTVIEW_ERR_COMM_TIMEOUT:    return "Communication timeout";
    case FTVIEW_ERR_AUTH_FAILED:     return "Authentication failed";
    case FTVIEW_ERR_DB_CORRUPT:      return "Database corrupt";
    case FTVIEW_ERR_NOT_IMPLEMENTED: return "Not implemented";
    default:                         return "Unknown error";
    }
}

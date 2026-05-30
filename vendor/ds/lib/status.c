#include "status.h"

const char *ds_status_name(ds_status_t status) {
  switch (status) {
  case DS_OK:
    return "DS_OK";
  case DS_NOT_FOUND:
    return "DS_NOT_FOUND";
  case DS_EXISTS:
    return "DS_EXISTS";
  case DS_EMPTY:
    return "DS_EMPTY";
  case DS_STOP:
    return "DS_STOP";
  case DS_ERR_ALLOC:
    return "DS_ERR_ALLOC";
  case DS_ERR_INVALID:
    return "DS_ERR_INVALID";
  case DS_ERR_NULL:
    return "DS_ERR_NULL";
  case DS_ERR_RANGE:
    return "DS_ERR_RANGE";
  case DS_ERR_OVERFLOW:
    return "DS_ERR_OVERFLOW";
  case DS_ERR_UNDERFLOW:
    return "DS_ERR_UNDERFLOW";
  case DS_ERR_STATE:
    return "DS_ERR_STATE";
  case DS_ERR_CALLBACK:
    return "DS_ERR_CALLBACK";
  case DS_ERR_UNSUPPORTED:
    return "DS_ERR_UNSUPPORTED";
  case DS_ERR_INTERNAL:
    return "DS_ERR_INTERNAL";
  default:
    return "DS_STATUS_UNKNOWN";
  }
}

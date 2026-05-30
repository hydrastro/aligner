#ifndef DS_STATUS_H
#define DS_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ds_status {
  DS_OK = 0,
  DS_NOT_FOUND = 1,
  DS_EXISTS = 2,
  DS_EMPTY = 3,
  DS_STOP = 4,

  DS_ERR_ALLOC = -1,
  DS_ERR_INVALID = -2,
  DS_ERR_NULL = -3,
  DS_ERR_RANGE = -4,
  DS_ERR_OVERFLOW = -5,
  DS_ERR_UNDERFLOW = -6,
  DS_ERR_STATE = -7,
  DS_ERR_CALLBACK = -8,
  DS_ERR_UNSUPPORTED = -9,
  DS_ERR_INTERNAL = -10
} ds_status_t;

#define DS_SUCCEEDED(status) ((status) >= 0)
#define DS_FAILED(status) ((status) < 0)

const char *ds_status_name(ds_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* DS_STATUS_H */

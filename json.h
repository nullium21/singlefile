#pragma once

#include <stdlib.h>
#include <string.h>

#if (!defined JSON_REALLOC && defined JSON_FREE) || (defined JSON_REALLOC && !defined JSON_FREE)
#error Either both JSON_REALLOC and JSON_FREE or none must be defined.
#endif

#ifndef JSON_REALLOC
#define JSON_REALLOC(ptr, sz) realloc(ptr, sz)
#endif

#ifndef JSON_FREE
#define JSON_FREE(ptr) JSON_FREE(ptr)
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct json_node {
  enum {
    json_node_object,
    json_node_array,
    json_node_string,
    json_node_number,
    json_node_boolean,
    json_node_null
  } type;
  union {
    struct json_array_node  *array;
    struct json_object_node *object;
    char const              *string;
    double                   number;
    int                      boolean;
  };
};

struct json_array_node {
  struct json_node value;
  struct json_array_node *next;
};

struct json_object_node {
  char const *key;
  struct json_node value;
  struct json_object_node *next;
};

struct json_node *json_parse_str(char const *);

#ifdef JSON_IMPLEMENTATION
int _json_parse_value(struct json_node *, char const *);

int _json_parse_num(struct json_node *ret, char const *str) {
  int len;
  double value;
  int ok = sscanf(str, "%lf %n", &value, &len);
  if (ok == 1) {
    ret->type = json_node_number;
    ret->number = value;
    return len;
  } else return ok ? -len : 0;
}

int _json_parse_str(struct json_node *ret, char const *str) {
  char const *start = str;
  if (*str++ != '"') return 0;

  char *buf = calloc(sizeof(char), strlen(str) + 1), *cur = buf;
  int newlen = 0;

  while (1) {
    char c = '\0';

    if (*str == '\\') switch (*++str) {
      case '"': case '\\': case '/':
        c = *str; break;
      case 'b': c = '\b'; break;
      case 'f': c = '\f'; break;
      case 'n': c = '\n'; break;
      case 'r': c = '\r'; break;
      case 't': c = '\t'; break;
      case 'u': {
        char hex[6] = { 0 }; // null-terminator between bytes
        if (!isxdigit(hex[0] = *++str)) return -(str - start);
        if (!isxdigit(hex[1] = *++str)) return -(str - start);
        if (!isxdigit(hex[3] = *++str)) return -(str - start);
        if (!isxdigit(hex[4] = *++str)) return -(str - start);
        newlen += 2;
        *cur++ = strtol( hex   , NULL, 16) & 0xff;
        *cur++ = strtol(&hex[3], NULL, 16) & 0xff;
        break;
      }
      default: return -newlen;
    } else if (*str == '"') { str++; break; }
    else c = *str++;

    if (c) { *cur++ = c; newlen++; }
  }

  buf = realloc(buf, newlen + 1);
  ret->type = json_node_string;
  ret->string = buf;
  return str - start;
}

int _json_parse_array(struct json_node *ret, char const *str) {
  char const *start = str;
  if (*str++ != '[') return 0;
  while (isspace(*str)) str++;

  struct json_array_node *arr = NULL, *cur = arr;

  while (1) {
    struct json_node n;
    int pv = _json_parse_value(&n, str);
    if (pv > 0) {
      str += pv;
      for (; cur && cur->next; cur = cur->next);
      if (cur == NULL) arr = cur = JSON_REALLOC(NULL, sizeof(struct json_array_node));
      else {
        cur->next = JSON_REALLOC(NULL, sizeof(struct json_array_node));
        cur = cur->next;
      }

      memcpy(&cur->value, &n, sizeof(struct json_node));

      while (isspace(*str)) str++;
      if (*str == ',') { str++; continue; }
    } else while (isspace(*str)) str++;
    if (*str == ']') { str++; break; }
    else return -(str - start);
  }

  ret->type = json_node_array;
  ret->array = arr;
  return str - start;
}

int _json_parse_object(struct json_node *ret, char const *str) {
  char const *start = str;
  if (*str++ != '{') return 0;

  struct json_object_node *obj = NULL, *cur = obj;

  while (1) {
    struct json_node kn, vn;
    while (isspace(*str)) str++;
    int pk = _json_parse_str(&kn, str);
    if (pk > 0) {
      str += pk;
      while (isspace(*str)) str++;
      if (*str != ':') return -(str - start); else str++;
      while (isspace(*str)) str++;
      int pv = _json_parse_value(&vn, str);
      if (pv <= 0) return -(str - start) + pv;
      str += pv;

      for (; cur && cur->next; cur = cur->next);
      if (cur == NULL) obj = cur = JSON_REALLOC(NULL, sizeof(struct json_object_node));
      else {
        cur->next = JSON_REALLOC(NULL, sizeof(struct json_object_node));
        cur = cur->next;
      }

      cur->key = kn.string;
      memcpy(&cur->value, &vn, sizeof(struct json_node));

      while (isspace(*str)) str++;
      if (*str == ',') { str++; continue; }
    } else while (isspace(*str)) str++;
    if (*str == '}') { str++; break; }
    else return -(str - start);
  }

  ret->type = json_node_object;
  ret->object = obj;
  return str - start;
}

int _json_parse_lit(struct json_node *ret, char const *str) {
  if (!strncmp(str, "true" , 4)) { ret->type = json_node_boolean; ret->boolean = 1; return 4; }
  if (!strncmp(str, "false", 5)) { ret->type = json_node_boolean; ret->boolean = 0; return 5; }
  if (!strncmp(str, "null" , 4)) { ret->type = json_node_null; return 4; }
  return 0;
}

int _json_parse_value(struct json_node *ret, char const *str) {
  char const *start = str;
  while (isspace(*str)) str++;

  int len;
  if ((len = _json_parse_lit   (ret, str)) > 0) return (str - start) + len;
  if ((len = _json_parse_str   (ret, str)) > 0) return (str - start) + len;
  if ((len = _json_parse_num   (ret, str)) > 0) return (str - start) + len;
  if ((len = _json_parse_array (ret, str)) > 0) return (str - start) + len;
  if ((len = _json_parse_object(ret, str)) > 0) return (str - start) + len;

  return -(str - start) + len;
}

struct json_node *json_parse_str(char const *str) {
  struct json_node *ret = JSON_REALLOC(NULL, sizeof(struct json_node));
  if (_json_parse_value(ret, str) < 0) {
    JSON_FREE(ret);
    return NULL;
  } else return ret;
}
#endif

#ifdef __cplusplus
}
#endif

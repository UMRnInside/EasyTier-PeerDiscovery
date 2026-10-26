// SPDX-License-Identifier: GPL-3.0-or-later

#include "node_json.h"
#include "net_platform.h"

#include <stdlib.h>
#include <string.h>

#define JSON_MAX_TOKENS 8192
#define JSON_MAX_DEPTH 32

typedef struct {
    const char *start;
    size_t length;
    size_t next;
    char type;
} json_token_t;

typedef struct {
    const char *p;
    const char *end;
    json_token_t *tokens;
    size_t count;
} json_parser_t;

static void whitespace(json_parser_t *p)
{
    while(p->p < p->end && strchr(" \t\r\n", *p->p) != NULL)
        p->p++;
}

static int hex_digit(unsigned char c)
{
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int unicode_escape(const char **p, const char *end, unsigned *value)
{
    unsigned code = 0;
    int i;

    if(end - *p < 4)
        return -1;
    for(i = 0; i < 4; i++) {
        int digit = hex_digit((unsigned char)*(*p)++);
        if(digit < 0)
            return -1;
        code = code * 16 + (unsigned)digit;
    }
    if(code >= 0xd800 && code <= 0xdbff) {
        unsigned low;
        if(end - *p < 6 || (*p)[0] != '\\' || (*p)[1] != 'u')
            return -1;
        *p += 2;
        low = 0;
        for(i = 0; i < 4; i++) {
            int digit = hex_digit((unsigned char)*(*p)++);
            if(digit < 0)
                return -1;
            low = low * 16 + (unsigned)digit;
        }
        if(low < 0xdc00 || low > 0xdfff)
            return -1;
        code = 0x10000 + (code - 0xd800) * 1024 + low - 0xdc00;
    } else if(code >= 0xdc00 && code <= 0xdfff) {
        return -1;
    }
    *value = code;
    return 0;
}

static int parse_value(json_parser_t *p, unsigned depth)
{
    json_token_t *token;
    const char *start;
    char type;

    whitespace(p);
    if(p->p == p->end || depth > JSON_MAX_DEPTH || p->count == JSON_MAX_TOKENS)
        return -1;
    token = &p->tokens[p->count++];
    token->start = start = p->p;
    token->type = type = *p->p++;
    if(type == '{' || type == '[') {
        char close = type == '{' ? '}' : ']';
        whitespace(p);
        if(p->p < p->end && *p->p == close) {
            p->p++;
        } else {
            for(;;) {
                whitespace(p);
                if(type == '{') {
                    if(p->p == p->end || *p->p != '"' || parse_value(p, depth + 1) != 0)
                        return -1;
                    whitespace(p);
                    if(p->p == p->end || *p->p++ != ':')
                        return -1;
                }
                if(parse_value(p, depth + 1) != 0)
                    return -1;
                whitespace(p);
                if(p->p == p->end)
                    return -1;
                if(*p->p == close) {
                    p->p++;
                    break;
                }
                if(*p->p++ != ',')
                    return -1;
            }
        }
    } else if(type == '"') {
        for(;;) {
            unsigned char c;
            if(p->p == p->end)
                return -1;
            c = (unsigned char)*p->p++;
            if(c == '"')
                break;
            if(c < 0x20)
                return -1;
            if(c == '\\') {
                unsigned code;
                if(p->p == p->end)
                    return -1;
                c = (unsigned char)*p->p++;
                if(c == 'u') {
                    if(unicode_escape(&p->p, p->end, &code) != 0)
                        return -1;
                } else if(c == 0 || strchr("\"\\/bfnrt", c) == NULL) {
                    return -1;
                }
            }
        }
    } else if(type == 't' || type == 'f' || type == 'n') {
        const char *literal = type == 't' ? "true" : type == 'f' ? "false" : "null";
        size_t length = strlen(literal);
        if((size_t)(p->end - start) < length || memcmp(start, literal, length) != 0)
            return -1;
        p->p = start + length;
    } else {
        p->p = start;
        if(*p->p == '-') p->p++;
        if(p->p == p->end) return -1;
        if(*p->p == '0') {
            p->p++;
        } else {
            if(*p->p < '1' || *p->p > '9') return -1;
            while(p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
        }
        if(p->p < p->end && *p->p == '.') {
            p->p++;
            if(p->p == p->end || *p->p < '0' || *p->p > '9') return -1;
            while(p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
        }
        if(p->p < p->end && (*p->p == 'e' || *p->p == 'E')) {
            p->p++;
            if(p->p < p->end && (*p->p == '+' || *p->p == '-')) p->p++;
            if(p->p == p->end || *p->p < '0' || *p->p > '9') return -1;
            while(p->p < p->end && *p->p >= '0' && *p->p <= '9') p->p++;
        }
    }
    token->length = (size_t)(p->p - start);
    token->next = p->count;
    return 0;
}

static int string_value(const json_token_t *t, char *out, size_t capacity)
{
    const char *p = t->start + 1;
    const char *end = t->start + t->length - 1;
    size_t length = 0;

    if(t->type != '"')
        return -1;
    while(p < end) {
        unsigned char c = (unsigned char)*p++;
        if(c == '\\') {
            c = (unsigned char)*p++;
            if(strchr("bfnrt", c) != NULL)
                return -1;
            if(c == 'u') {
                unsigned code;
                if(unicode_escape(&p, end, &code) != 0 || code < 0x20 || code > 0x7e)
                    return -1;
                c = (unsigned char)code;
            }
        }
        if(c < 0x20 || c > 0x7e || length + 1 >= capacity)
            return -1;
        out[length++] = (char)c;
    }
    out[length] = '\0';
    return 0;
}

static size_t member(const json_token_t *tokens, size_t object, const char *name)
{
    size_t i;
    size_t found = 0;
    if(tokens[object].type != '{')
        return 0;
    for(i = object + 1; i < tokens[object].next; i = tokens[i + 1].next) {
        char key[64];
        if(string_value(&tokens[i], key, sizeof(key)) == 0 && strcmp(key, name) == 0) {
            if(found != 0)
                return 0;
            found = i + 1;
        }
    }
    return found;
}

int node_json_parse(const char *json, size_t length, node_json_t *node)
{
    json_token_t *tokens = calloc(JSON_MAX_TOKENS, sizeof(*tokens));
    json_parser_t parser = {json, json + length, tokens, 0};
    size_t stun, ips, listeners, field, i;
    int rc = -1;

    memset(node, 0, sizeof(*node));
    if(tokens == NULL)
        return -1;
    if(memchr(json, '\0', length) != NULL || parse_value(&parser, 0) != 0)
        goto done;
    whitespace(&parser);
    if(parser.p != parser.end || tokens[0].type != '{')
        goto done;
    stun = member(tokens, 0, "stun_info");
    listeners = member(tokens, 0, "listeners");
    if(stun == 0 || tokens[stun].type != '{' ||
       listeners == 0 || tokens[listeners].type != '[')
        goto done;
    field = member(tokens, stun, "tcp_nat_type");
    if(field != 0 && string_value(&tokens[field], node->tcp_nat, sizeof(node->tcp_nat)) != 0)
        goto done;
    field = member(tokens, stun, "udp_nat_type");
    if(field != 0 && string_value(&tokens[field], node->udp_nat, sizeof(node->udp_nat)) != 0)
        goto done;
    ips = member(tokens, stun, "public_ip");
    if(ips != 0) {
        if(tokens[ips].type != '[')
            goto done;
        for(i = ips + 1; i < tokens[ips].next; i = tokens[i].next) {
            char address[64];
            unsigned char binary[16];
            if(string_value(&tokens[i], address, sizeof(address)) != 0)
                goto done;
            if(node->public_ipv4[0] == '\0' && inet_pton(AF_INET, address, binary) == 1)
                strcpy(node->public_ipv4, address);
            if(node->public_ipv6[0] == '\0' && inet_pton(AF_INET6, address, binary) == 1)
                strcpy(node->public_ipv6, address);
        }
    }
    for(i = listeners + 1; i < tokens[listeners].next; i = tokens[i].next) {
        char listener[128];
        if(string_value(&tokens[i], listener, sizeof(listener)) != 0)
            goto done;
        if(strncmp(listener, "tcp://", 6) == 0) {
            char *target = listener[6] == '[' ? node->tcp6_listener : node->tcp4_listener;
            if(target[0] == '\0')
                strcpy(target, listener);
        }
        if(strncmp(listener, "udp://", 6) == 0) {
            char *target = listener[6] == '[' ? node->udp6_listener : node->udp4_listener;
            if(target[0] == '\0')
                strcpy(target, listener);
        }
        if(strncmp(listener, "quic://", 7) == 0) {
            char *target = listener[7] == '[' ? node->quic6_listener : node->quic4_listener;
            if(target[0] == '\0')
                strcpy(target, listener);
        }
    }
    rc = 0;
done:
    free(tokens);
    if(rc != 0)
        memset(node, 0, sizeof(*node));
    return rc;
}

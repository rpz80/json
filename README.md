https://travis-ci.org/rpz80/json.svg?branch=master
# json
Pure C json serialization library. For the complete API refer to the json.h. Here is a couple of basic exapmles.
## Parsing
``` C
const int kErrorBufSize = 1024;
int retCode;
int len;
struct JsonVal val;
struct JsonVal *inner;
char errBuf[kErrorBufSize];

const char *rawSource = "{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": [{\"myKey4\":true}, 5]}}";

val = jsonParseString(rawSource, errBuf, kErrorBufSize);

if (*errBuf != 0) // Parsing failed
{
    fprintf(stderr, "%s\n", errBuf);
    JsonVal_destroy(&val);
    return;
}

inner = JsonVal_getObjectValueByKey(&val, "myObj");
assert(JsonVal_isObject(inner) == 1);

inner = JsonVal_getObjectValueByKey(inner, "myKey2");
assert(JsonVal_isArray(inner) == 1);

void JsonVal_forEachArrayElement(inner, NULL, &workWithArrayElem);

JsonVal_destroy(&val);

```
## Building && writing
``` C
static writeToSocket(void *ctx, void *buf, int len)
{
    struct Socket *socket = (struct Socket *)ctx;
    if (Socket_write(socket, buf, len) != 0)
        perror("Write to socket filed");
}

void buildAndSend()
{
    struct JsonVal top;
    struct JsonVal *subArray;
    struct JsonVal *subArrayObj;
    struct Socket socket;
    char out[1024];

    top = jsonCreateObject();
    JsonVal_objectAddString(&top, "key1", "string1");
    JsonVal_objectAddObject(&top, "key2");

    subArray = JsonVal_objectAddArray(JsonVal_getObjectValueByKey(&top, "key2"), "subKey1");

    JsonVal_arrayAddNumber(subArray, 42);
    JsonVal_arrayAddTrue(subArray);
    JsonVal_arrayAddFalse(subArray);

    subArrayObj = JsonVal_arrayAddObject(subArray);
    JsonVal_objectAddNull(subArrayObj, "nullKey");

    JsonVal_writeString(&top, out, 1024);

    assert(strcmp(out, "{\"key1\":\"string1\",\"key2\":{\"subKey1\":[42,true,false,{\"nullKey\":null}]}}") == 0);
    
    if (Socket_open(&socket, "myhost.com:8791") == 0)
        JsonVal_write(&top, &socket, &writeToSocket);

    JsonVal_destroy(&top);
}

```

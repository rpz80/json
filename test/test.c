#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <json/json.h>

#define ASSERT_TRUE(expr) \
    do { \
        //assert((expr)); \
        if (!(expr)) {\
            exit(-1); \
            fprintf(stderr, "%s:%d:%s. ASSERT_TRUE '%s' failed\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, #expr); \
        } \
    } while (0)

static const int kErrorBufSize = 1024;

static char *readFileToString(const char *path)
{
    int bufSize = 512;
    int bytesRead;
    int totalRead = 0;
    int bytesToRead;
    char *buf = NULL;
    FILE *f;

    f = fopen(path, "rb");
    if (f == NULL)
        return NULL;

    buf = malloc(bufSize);

    while (1)
    {
        bytesToRead = bufSize - totalRead;
        bytesRead = fread(buf + totalRead, 1, bytesToRead, f);
        if (bytesRead == 0)
            break;

        totalRead += bytesRead;
        if (totalRead == bufSize)
        {
            bufSize *= 2;
            buf = realloc(buf, bufSize);
        }
    }

    if (totalRead == bufSize)
        buf = realloc(buf, bufSize + 1);

    buf[totalRead] = '\0';
    fclose(f);

    return buf;
}

static int loadJsonFromFile(const char *path, struct JsonVal* value)
{
    char *content;
    char errorBuf[1024];

    content = readFileToString(path);
    if (content == NULL)
        return -1;

    memset(content, 0, 1024);
    *value = jsonParseString(content, errorBuf, 1024);
    free(content);

    if (strlen(errorBuf) != 0)
        return -1;

    return 0;
}

int cmpSkipSpaces(const char *str1, const char *str2)
{
    while(*str1 || *str2)
    {
        while (*str1 && isspace(*str1))
            ++str1;

        while (*str2 && isspace(*str2))
            ++str2;

        if (*str1 != *str2)
            return -1;

        if (*str1)
            ++str1;

        if (*str2)
            ++str2;
    }

    return 0;
}

static void testVsString(const char *pattern, int mustFail);

static void testVsFile(const char *path)
{
    char *rawContent;

    rawContent = readFileToString(path);
    ASSERT_TRUE(rawContent);

    testVsString(rawContent, 0);
    free(rawContent);
}

static void testVsString(const char *rawSource, int mustFail)
{
    int retCode;
    int len;
    char *serializedContent;
    struct JsonVal val;
    char errBuf[kErrorBufSize];

    val = jsonParseString(rawSource, errBuf, kErrorBufSize);
    ASSERT_TRUE(mustFail ? *errBuf : !*errBuf);

    if (*errBuf != 0)
    {
        JsonVal_destroy(&val);
        return;
    }

    len = strlen(rawSource);
    serializedContent = malloc(len);
    JsonVal_writeString(&val, serializedContent, len);

    retCode = cmpSkipSpaces(rawSource, serializedContent);
    ASSERT_TRUE(mustFail ? (retCode != 0) : (retCode == 0));

    free(serializedContent);
    JsonVal_destroy(&val);
}

enum MatchState {
    single,
    asteriks,
    any,
    group,
    notGroup,
    errorState
};

static enum MatchState getState(const char **mask, char *buf)
{
    enum MatchState result;

    if (!**mask)
        return errorState;

    switch (**mask)
    {
        case '*':
            result = asteriks;
            break;
        case '?':
            result = any;
            break;
        case '[':
            ++*mask;
            result = group;
            if (**mask == '!')
            {
                result = notGroup;
                ++*mask;
            }
            while (**mask != ']')
            {
                if (!**mask)
                    return errorState;
                *buf = **mask;
                ++buf;
                ++*mask;
            }
            *buf = '\0';
            break;
        default:
            result = single;
            *buf = **mask;
            break;
    }

    ++*mask;

    return result;
}

static int checkGroup(char c, const char *buf, int inGroup)
{
    int result = 0;

    while (*buf)
    {
        if (*buf == c)
        {
            result = 1;
            break;
        }
        ++buf;
    }

    if ((result && inGroup) || (!result && !inGroup))
        return 0;

    return -1;
}

static int matchMaskImpl(const char *mask, const char *string, char *buf, enum MatchState currentState)
{
    const char *tmpMask;
    int needShift;

    while (1)
    {
        if (!*mask)
        {
            if (!*string || currentState == asteriks)
                return 0;
            else if (*string && strlen(string) > 1)
                return -1;
        }
        else if (!*string)
        {
            if (currentState == asteriks)
            {
                currentState = getState(&mask, buf);
                continue;
            }
            else
                return -1;
        }

        needShift = 1;

        switch (currentState)
        {
            case errorState:
                return -1;
            case single:
                if (*buf != *string)
                    return -1;
                break;
            case any:
                break;
            case group:
                if (checkGroup(*string, buf, 1) != 0)
                    return -1;
                break;
            case notGroup:
                if (checkGroup(*string, buf, 0) != 0)
                    return -1;
                break;
            case asteriks:
                tmpMask = mask;
                if (matchMaskImpl(mask, ++string, buf, asteriks) == 0)
                    return 0;
                needShift = 0;
                break;

        }
        currentState = getState(&mask, buf);
        if (needShift && *string)
            ++string;
    }

    return -1;
}

static int matchMask(const char *mask, const char *string)
{
    char buf[128];
    enum MatchState state;

    memset(buf, 0, sizeof(buf));
    if (!*mask)
    {
        if (!*string)
            return 0;
        else
            return -1;
    }

    state = getState(&mask, buf);
    if (state == errorState)
        return -1;

    return matchMaskImpl(mask, string, buf, state);
}

static char *pathJoin(const char *str1, const char *str2)
{
    int len1, len2;
    char *result;

    len1 = strlen(str1);
    len2 = strlen(str2);
    result = malloc(len1 + len2 + 2);

    if (!result)
        return NULL;

    if (result)
        strcpy(result, str1);

    if (result[len1 - 1] != '/')
    {
        result[len1] = '/';
        len1 += 1;
    }

    strcpy(result + len1, str2);
    result[len1 + len2] = '\0';

    return result;
}

static int forEachFile(
        const char *path,
        const char *mask,
        void (*action)(const char *fileName))
{
    struct dirent *entry;
    DIR *dp;
    char *fullPath;

    dp = opendir(path);
    if (dp == NULL)
        return -1;

    while ((entry = readdir(dp)))
        if (matchMask(mask, entry->d_name) == 0)
        {
            fullPath = pathJoin(path, entry->d_name);
            ASSERT_TRUE(fullPath);
            if (!fullPath)
                continue;
            action(fullPath);
            free(fullPath);
        }

    closedir(dp);
    return 0;
}

static void forEachString(
        const char **source,
        int *mustFailArray,
        int sourceLen,
        void (*action)(const char *, int))
{
    int i;

    for (i = 0; i < sourceLen; ++i)
        action(source[i], mustFailArray[i]);
}

static void testMatchFunc()
{
    ASSERT_TRUE(matchMask("*", "abc.df") == 0);
    ASSERT_TRUE(matchMask("*.d?", "c.df") == 0);
    ASSERT_TRUE(matchMask("a*c.?f", "abc.df") == 0);
    ASSERT_TRUE(matchMask("a*", "abc.df") == 0);
    ASSERT_TRUE(matchMask("[kdar]b?.d*", "abc.df") == 0);
    ASSERT_TRUE(matchMask("b?.d*", "abc.df") != 0);
    ASSERT_TRUE(matchMask("[kdarb?.d*", "abc.df") != 0);
}

static void testAccessors()
{
    char errorBuf[kErrorBufSize];
    struct JsonVal val;
    struct JsonVal *inner;

    val = jsonParseString("{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": false}}", errorBuf, kErrorBufSize);
    ASSERT_TRUE(!*errorBuf);
    inner = JsonVal_getObjectValueByKey(&val, "myObj");
    ASSERT_TRUE(inner);
    ASSERT_TRUE(inner->type = jsonObjectT);
    inner = JsonVal_getObjectValueByKey(inner, "myKey1");
    ASSERT_TRUE(inner->type == jsonNumberT);
    ASSERT_TRUE(inner->u.number - 123.98 < 0.00001 && -(inner->u.number - 123.98) < 0.00001);

    JsonVal_destroy(&val);
}

static void testBuildObjectFunctions()
{
    struct JsonVal top;
    struct JsonVal *subArray;
    struct JsonVal *subArrayObj;
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

    ASSERT_TRUE(strcmp(out, "{\"key1\":\"string1\",\"key2\":{\"subKey1\":[42,true,false,{\"nullKey\":null}]}}") == 0);

    JsonVal_destroy(&top);
}

int main()
{
    testMatchFunc();

    static const char *testJsons[] =
            {"{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": false}}",
             "{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": [1, 2, 3]}}",
             "{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": [\"myKey3\":{\"myKey4\":true}, 5]}}",
             "{\"myObj\": {\"myKey1\": 123.98, \"myKey2\": [{\"myKey4\":true}, 5]}}"};

    static int mustFailArray[] = {0, 0, 1, 0};

    forEachString(testJsons, mustFailArray, sizeof(mustFailArray) / sizeof(*mustFailArray), testVsString);
    forEachFile("../samples", "sample*.json", testVsFile);
    testAccessors();
    testBuildObjectFunctions();

    return 0;
}

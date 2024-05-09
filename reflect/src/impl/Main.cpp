#include <clang-c/Index.h>
#include <clang-c/CXString.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <io.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <unordered_map>

#include "CommonDefines.h"
#include "Common.h"
#include "AST.h"

#define OS_WINDOWS

void Logger(void* data, const char* fmt, va_list* args)
{
    vprintf(fmt, *args);
}

void AssertHandler(void* data, const char* file, const char* func, u32 line, const char* assertStr, const char* fmt, va_list* args)
{
    LogPrint("[Assertion failed] Expression (%s) result is false\nFile: %s, function: %s, line: %d.\n", assertStr, file, func, (int)line);
    if (args)
    {
        GlobalLogger(GlobalLoggerData, fmt, args);
    }
    BreakDebug();
}

LoggerFn* GlobalLogger = Logger;
void* GlobalLoggerData = nullptr;

AssertHandlerFn* GlobalAssertHandler = AssertHandler;
void* GlobalAssertHandlerData = nullptr;

struct VisitorData
{
    std::vector<AstNode*> stack;
    std::vector<const char*> attributesStack;
    std::unordered_map<std::string, AstNode*> typesTable;
    AstNode rootNode;
};

struct ASTAttribEntry
{
    CXCursor parent;
    CXCursor attrib;
};

struct ASTAttribCollection
{
    std::vector<ASTAttribEntry> enums;
    std::vector<ASTAttribEntry> structs;
    std::vector<ASTAttribEntry> fields;
};

BuiltInType ClangTypeToBuiltIn(CXTypeKind clangType)
{
    switch (clangType)
    {
    case CXType_Short: return BuiltInType_SignedShort;
    case CXType_UShort: return BuiltInType_UnsignedShort;
    case CXType_Int: return BuiltInType_SignedInt;
    case CXType_UInt: return BuiltInType_UnsignedInt;
    case CXType_Long: return BuiltInType_SignedLong;
    case CXType_ULong: return BuiltInType_UnsignedLong;
    case CXType_LongLong: return BuiltInType_SignedLongLong;
    case CXType_ULongLong: return BuiltInType_UnsignedLongLong;
    case CXType_SChar: return BuiltInType_SignedChar;
    case CXType_Char_S: return BuiltInType_Char;
    case CXType_UChar: return BuiltInType_UnsignedChar;
    case CXType_Char_U: return BuiltInType_Char;
    case CXType_Bool: return BuiltInType_Bool;
    case CXType_Char16: return BuiltInType_Char16;
    case CXType_Char32: return BuiltInType_Char32;
    case CXType_WChar: return BuiltInType_Wchar;
    case CXType_Float: return BuiltInType_Float;
    case CXType_Double: return BuiltInType_Double;
    case CXType_LongDouble: return BuiltInType_LongDouble;
    default: return BuiltInType_Unknown;
    }
}

AstNode* PushNewChild(VisitorData* data)
{
    AstNode** children = data->stack.back()->children;
    auto childrenVector = (std::vector<AstNode*>*)children;

    auto node = (AstNode*)malloc(sizeof(AstNode));
    memset(node, 0, sizeof(AstNode));

    childrenVector->push_back(node);
    data->stack.push_back(node);
    return node;
}

void PopNode(VisitorData* data)
{
    data->stack.pop_back();
}

bool IsSpace(char c)
{
    bool result = false;

    if (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
    {
        result = true;
    }

    return result;
}

bool IsAnonymousType(const char* spelling)
{
    // Hack
    return strstr(spelling, "anonymous at");
}

const char* ExtractTypeName(const char* string)
{
    if (IsAnonymousType(string))
    {
        return string;
    }

    const char* lastWordPosition = string;

    string++;

    while (*string != 0)
    {
        if (!IsSpace(*string) && IsSpace(*(string - 1)))
        {
            lastWordPosition = string;
        }

        string++;
    }

    return lastWordPosition;
}

bool CheckTypeIsAlreadyProcessed(VisitorData* data, const char* name)
{
    auto it = data->typesTable.find(std::string(name));
    return it != data->typesTable.end();
}

AstNode* TryFindProcessedType(VisitorData* data, const char* name)
{
    AstNode* result = nullptr;

    auto it = data->typesTable.find(std::string(name));
    if (it != data->typesTable.end())
    {
        result = it->second;
    }

    return result;
}

void RegisterProcessedType(VisitorData* data, const char* name, AstNode* typeNode)
{
    data->typesTable[std::string(name)] = typeNode;
}

const char* TryExtractMetaprogramAttribute(CXCursor attribCursor)
{
    bool visible = false;
    static const char *metaprogramKeyword = "__metaprogram";
    static const int metaprogramKeywordSize = sizeof("__metaprogram");

    CXString attribSpelling = clang_getCursorSpelling(attribCursor);
    const char *attribSpellingStr = clang_getCString(attribSpelling);

    const char *keyword = strstr(attribSpellingStr, metaprogramKeyword);
    if (keyword)
    {
        keyword += metaprogramKeywordSize;
        return keyword;
    }

    return nullptr;
}

CXChildVisitResult ChildAttrbutesVisitor(CXCursor cursor, CXCursor parent, CXClientData _data)
{
    auto data = (VisitorData*)_data;

    CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursorKind parentKind = clang_getCursorKind(parent);

    if (kind == CXCursor_AnnotateAttr)
    {
        const char* attributeString = TryExtractMetaprogramAttribute(cursor);
        if (attributeString)
        {
            data->attributesStack.push_back(attributeString);
        }
    }

    return CXChildVisit_Continue;
}

enum struct AttributeTokenType
{
    Error,
    Identifier,
    Number,
    String,
    OpenBrace,
    CloseBrace,
    Colon,
    Comma,
    End
};

static const char* AttributeTokenType_Strings[] =
{
    "Error",
    "Identifier",
    "Number",
    "String",
    "OpenBrace",
    "CloseBrace",
    "Colon",
    "Comma",
    "End"
};

struct AttributeToken
{
    AttributeTokenType type;
    const char* str;
    u32 length;
    const char* errorPosition;
};

struct AttributesTokenizer
{
    const char* at;
    std::vector<AttributeToken> tokens;
};

void EatSpace(AttributesTokenizer* tokenizer)
{
    while (*tokenizer->at && IsSpace(*tokenizer->at)) tokenizer->at++;
}

bool IsNumber(char c)
{
    bool result = c >= '0' && c <= '9';
    return result;
}

bool IsAlpha(char c)
{
    bool result = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    return result;
}

bool IsSeparator(char c)
{
    bool result = c == ':' || c == ',' || c == '(' || c == ')' || c == '\"';
    return result;
}

void TokenizeAttribute(AttributesTokenizer* tokenizer)
{
    while (true)
    {
        EatSpace(tokenizer);

        if (*tokenizer->at == 0)
        {
            AttributeToken token = {};
            token.type = AttributeTokenType::End;
            tokenizer->tokens.push_back(token);
            break;
        }

        if (*tokenizer->at == ',')
        {
            AttributeToken token = {};
            token.type = AttributeTokenType::Comma;
            token.str = tokenizer->at;
            token.length = 1;
            tokenizer->at++;
            tokenizer->tokens.push_back(token);
            continue;
        }

        if (*tokenizer->at == ':')
        {
            AttributeToken token = {};
            token.type = AttributeTokenType::Colon;
            token.str = tokenizer->at;
            token.length = 1;
            tokenizer->at++;
            tokenizer->tokens.push_back(token);
            continue;
        }

        if (*tokenizer->at == '(')
        {
            AttributeToken token = {};
            token.type = AttributeTokenType::OpenBrace;
            token.str = tokenizer->at;
            token.length = 1;
            tokenizer->at++;
            tokenizer->tokens.push_back(token);
            continue;
        }

        if (*tokenizer->at == ')')
        {
            AttributeToken token = {};
            token.type = AttributeTokenType::CloseBrace;
            token.str = tokenizer->at;
            token.length = 1;
            tokenizer->at++;
            tokenizer->tokens.push_back(token);
            continue;
        }

        // TODO: Recognize quotes with escape symbol.
        if (*tokenizer->at == '\"')
        {
            tokenizer->at++;
            auto begin = tokenizer->at;
            while (*tokenizer->at && *tokenizer->at != '\"') tokenizer->at++;
            if (*tokenizer->at != '\"')
            {
                static const char* error = "Unexpected end of string";
                static u32 errorLength = (u32)strlen(error);

                AttributeToken token = {};
                token.type = AttributeTokenType::Error;
                token.str = error;
                token.length = errorLength;
                token.errorPosition = begin - 1;
                tokenizer->tokens.push_back(token);
                break;
            }

            AttributeToken token = {};
            token.type = AttributeTokenType::String;
            token.str = begin;
            token.length = (u32)((uptr)tokenizer->at - (uptr)begin);

            tokenizer->at++;
            tokenizer->tokens.push_back(token);
            continue;
        }

        if (IsNumber(*tokenizer->at))
        {
            auto begin = tokenizer->at;
            bool isStillNumber = true;
            while (*tokenizer->at && !IsSpace(*tokenizer->at) && !IsSeparator(*tokenizer->at))
            {
                if (!IsNumber(*tokenizer->at))
                {
                    isStillNumber = false;
                }

                tokenizer->at++;
            }

            AttributeToken token = {};

            if (isStillNumber)
            {
                token.type = AttributeTokenType::Number;
                token.str = begin;
                token.length = (u32)((uptr)tokenizer->at - (uptr)begin);
                tokenizer->tokens.push_back(token);
            }
            else
            {
                static const char* error = "Number contains non-digit characters";
                static u32 errorLength = (u32)strlen(error);

                token.type = AttributeTokenType::Error;
                token.str = error;
                token.length = errorLength;
                token.errorPosition = begin;
                tokenizer->tokens.push_back(token);

                break;
            }

            continue;
        }

        if (IsAlpha(*tokenizer->at))
        {
            auto begin = tokenizer->at;
            while (*tokenizer->at && (IsAlpha(*tokenizer->at) || IsNumber(*tokenizer->at))) tokenizer->at++;

            AttributeToken token = {};
            token.type = AttributeTokenType::Identifier;
            token.str = begin;
            token.length = (u32)((uptr)tokenizer->at - (uptr)begin);

            tokenizer->tokens.push_back(token);
            continue;
        }

        static const char* error = "Unexpected character";
        static u32 errorLength = (u32)strlen(error);

        AttributeToken token = {};
        token.type = AttributeTokenType::Error;
        token.str = error;
        token.length = errorLength;
        token.errorPosition = tokenizer->at;
        tokenizer->tokens.push_back(token);

        break;
    }
}

char* ExtractString(const char* str, u32 len)
{
    auto mem = (char*)malloc(len + 1);
    memcpy(mem, str, len);
    mem[len] = 0;
    return mem;
}

#if 0
bool ResolveAttributeParams(AttributeToken* tokens, u32 tokensCount, AttributeParameter** params, u32* paramsCount)
{
    u32 index = 0;
    while (true)
    {
        if ((tokensCount - index) == 0) break;
        if ((tokensCount - index) < 3)
        {
            LogPrint("Error: Unexpected end of attribute parameters list\n");
            return false;
        }

        if (tokens[index + 0] == AttributeTokenType::Identifier &&
            tokens[index + 1] == AttributeTokenType::Colon &&
            (tokens[index + 2] == AttributeTokenType::Number ||
             tokens[index + 2] == AttributeTokenType::String ||
             tokens[index + 2] == AttributeTokenType::Bool))
        {
            // TODO: collect params here
            auto param
        }
        else
        {
            LogPrint("Error: Unexpected end of attribute parameter syntax\n");
            return false;
        }
    }
}

AttributeData* ResolveAttribute(AttributeToken* tokens, u32 tokensCount, const char* attributeString)
{
    assert(tokensCount > 1);

    if (tokens[0].type == AttributeTokenType::Identifier)
    {
        // Identifier only attribute
        if (tokens[1].type == AttributeTokenType::End)
        {
            auto data = (AttributeData*)malloc(sizeof(AttributeData));
            memset(data, 0, sizeof(AttributeData));
            data->attributeName = ExtractString(tokens[1].str, tokens[1].length);
            return data;
        }
        // Identifier with params
        else if (tokens[1].type == AttributeTokenType::OpenBrace)
        {
            u32 closingBraceIndex = 2;
            while (tokens[closingBraceIndex] != AttributeTokenType::End || tokens[closingBraceIndex] != AttributeTokenType::CloseBrace)
            {
                closingBraceIndex++;
            }

            if (tokens[closingBraceIndex] == AttributeTokenType::CloseBrace)
            {
                auto beginIndex = 2;
                auto paramsLength = closingBraceIndex - beginIndex;
            }
            else
            {
                // TODO: better error handling.
                LogPrint("Error: Unexpected end of attribute parameters list: %s\n", attributeString);
                return nullptr;
            }
        }
        else
        {
            // TODO: better error handling.
            LogPrint("Error: Unexpected token after attribute name: %s\n", attributeString);
            return nullptr;
        }
    }
    else
    {
        // TODO: better error handling.
        LogPrint("Error: attribute must begin with identifier: %s\n", attributeString);
        return nullptr;
    }
}

#endif
struct ParseAttributeResult
{
    bool succeed;
    AttributeData* data;
};

ParseAttributeResult ParseAttribute(const char* attribString)
{
    ParseAttributeResult result = {};

    // Cache tokenizer.
    AttributesTokenizer tokenizer = {};
    tokenizer.at = attribString;
    TokenizeAttribute(&tokenizer);

    if (tokenizer.tokens.size() == 0 || (tokenizer.tokens.size() == 1 && tokenizer.tokens.back().type == AttributeTokenType::End))
    {
        result.succeed = true;
        return result;
    }

    // Something is wrong
    if (tokenizer.tokens.back().type != AttributeTokenType::End)
    {
        if (tokenizer.tokens.back().type == AttributeTokenType::Error)
        {
            auto& token = tokenizer.tokens.back();
            std::string temp(attribString);
            auto pos = (uptr)token.errorPosition - (uptr)attribString;
            // printf red color.
            temp.insert(pos, "\033[0;31m|>\033[0m");
            // TODO: text position
            LogPrint("Attribute parsing error: %.*s: %s\n", token.length, token.str, temp.c_str());
        }
        else
        {
            LogPrint("Unexpected end of attribute: %s\n", attribString);
        }

        result.succeed = false;
        return result;
    }

    for (auto token : tokenizer.tokens)
    {
        printf("%s\t%.*s\n", AttributeTokenType_Strings[(u32)token.type], token.length, token.str);
    }

    return result;
}

void ExtractAttributes(CXCursor cursor, VisitorData* data, AttributeData** outAttrbutes, u32* outAttrbutesCount)
{
    // TODO: Uncomment when implementing attributes!

    *outAttrbutes = nullptr;
    *outAttrbutesCount = 0;

    data->attributesStack.clear();
    clang_visitChildren(cursor, ChildAttrbutesVisitor, data);

    if (data->attributesStack.size())
    {

        *outAttrbutesCount = (u32)data->attributesStack.size();
        *outAttrbutes = (AttributeData *)malloc(sizeof(AttributeData) * *outAttrbutesCount);
        for (u32 i = 0; i < *outAttrbutesCount; i++)
        {
            //ParseAttribute(data->attributesStack[i]);
            (*outAttrbutes)[i].attributeName = data->attributesStack[i];
        }
    }
}

CXChildVisitResult EnumVisitor(CXCursor cursor, CXCursor parent, CXClientData _data)
{
    auto data = (VisitorData*)_data;
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_EnumConstantDecl)
    {
        AstNode* node = PushNewChild(data);
        node->type = AstNodeType_EnumConstant;
        auto nodeData = (EnumConstantData*)malloc(sizeof(EnumConstantData));
        node->data = nodeData;

        CXString name = clang_getCursorSpelling(cursor);
        const char* nameStr = clang_getCString(name);

        unsigned long long uValue = clang_getEnumConstantDeclUnsignedValue(cursor);
        long long sValue = clang_getEnumConstantDeclValue(cursor);

        nodeData->name = nameStr;
        nodeData->signedValue = sValue;
        nodeData->unsignedValue = uValue;

        ExtractAttributes(cursor, data, &nodeData->attributes.attributes, &nodeData->attributes.attributesCount);

        PopNode(data);
    }
    else if (kind != CXCursor_AnnotateAttr)
    {
        LogPrint("Unexpected syntax in enum\n.");
    }

    return CXChildVisit_Continue;
}

TypeInfo* ResolveFieldType(CXType type, VisitorData* data)
{
    BuiltInType builtInType = ClangTypeToBuiltIn(type.kind);

    if (builtInType != BuiltInType_Unknown)
    {
        auto typeInfo = (TypeInfo*)malloc(sizeof(TypeInfo));
        memset(typeInfo, 0, sizeof(TypeInfo));

        typeInfo->kind = TypeKind_BuiltIn;
        typeInfo->builtInType = builtInType;
        return typeInfo;
    }

    if (type.kind == CXType_Pointer)
    {
        auto typeInfo = (TypeInfo *)malloc(sizeof(TypeInfo));
        memset(typeInfo, 0, sizeof(TypeInfo));

        typeInfo->kind = TypeKind_Pointer;

        CXType pointeeType = clang_getPointeeType(type);

        typeInfo->underlyingType = ResolveFieldType(pointeeType, data);
        return typeInfo;
    }

    // Proto is the function with parameters, NoProto - has no parameters.
    if (type.kind == CXType_FunctionProto || type.kind == CXType_FunctionNoProto)
    {
        auto typeInfo = (TypeInfo *)malloc(sizeof(TypeInfo));
        memset(typeInfo, 0, sizeof(TypeInfo));

        typeInfo->kind = TypeKind_FunctionProto;
        return typeInfo;
    }

    if (type.kind == CXType_ConstantArray ||
        type.kind == CXType_IncompleteArray)
    {
        auto typeInfo = (TypeInfo *)malloc(sizeof(TypeInfo));
        memset(typeInfo, 0, sizeof(TypeInfo));

        CXType elementType = clang_getArrayElementType(type);

        typeInfo->kind = TypeKind_Array;

        if (type.kind == CXType_ConstantArray)
        {
            long long arraySize = clang_getArraySize(type);
            Assert(arraySize >= 0);

            typeInfo->arrayHasSize = true;
            typeInfo->arrayCount = (u32)arraySize;
        }
        else
        {
            typeInfo->arrayHasSize = false;
        }

        typeInfo->underlyingType = ResolveFieldType(elementType, data);
        return typeInfo;
    }

    if (type.kind == CXType_Typedef)
    {
        CXType actualType = clang_getCanonicalType(type);
        return ResolveFieldType(actualType, data);
    }

    if (type.kind == CXType_Record || type.kind == CXType_Enum)
    {
        auto typeInfo = (TypeInfo *)malloc(sizeof(TypeInfo));
        memset(typeInfo, 0, sizeof(TypeInfo));

        CXString typeSpelling = clang_getTypeSpelling(type);
        const char *typeSpellingStr = clang_getCString(typeSpelling);
        const char *typeName = ExtractTypeName(typeSpellingStr);

        AstNode* typeNode = TryFindProcessedType(data, typeName);

        if (typeNode != nullptr)
        {
            typeInfo->kind = type.kind == CXType_Record ? TypeKind_Struct : TypeKind_Enum;
            typeInfo->resolvedTypeNode = typeNode;
            clang_disposeString(typeSpelling);
        }
        else
        {
            typeInfo->kind = TypeKind_Unresolved;
            typeInfo->unresolvedTypeName = typeName;
        }

        return typeInfo;
    }

    if (type.kind == CXType_Elaborated)
    {
        CXType actualType = clang_Type_getNamedType(type);
        return ResolveFieldType(actualType, data);
    }

    return nullptr;
}

void MakeChildrenVector(AstNode* node)
{
    // Put vector in children array pointer. We will collect children here
    node->children = (AstNode**)(new std::vector<AstNode*>());
}

void ConvertChildrenVectorToArray(AstNode* node)
{
    // Convert children vector to plain array
    auto childrenVector = (std::vector<AstNode*>*)node->children;
    u32 childrenCount = (u32)childrenVector->size();
    u32 arraySize = sizeof(AstNode*) * childrenCount;

    node->children = (AstNode**)malloc(arraySize);
    node->childrenCount = (u32)childrenCount;
    memcpy(node->children, childrenVector->data(), arraySize);
    delete childrenVector;
}

CXChildVisitResult StructVisitor(CXCursor cursor, CXCursor parent, CXClientData _data);

void TypeVisitor(CXCursor cursor, VisitorData* data)
{
    CXCursorKind kind = clang_getCursorKind(cursor);

    switch (kind)
    {
    case CXCursor_EnumDecl:
    {
        if (!clang_isCursorDefinition(cursor))
        {
            break;
        }

        CXType type = clang_getCursorType(cursor);
        CXString spelling = clang_getTypeSpelling(type);
        const char *spellingStr = clang_getCString(spelling);
        bool anonymous = IsAnonymousType(spellingStr);
        const char *typeName = ExtractTypeName(spellingStr);

        if (CheckTypeIsAlreadyProcessed(data, spellingStr))
        {
            clang_disposeString(spelling);
            break;
        }

        auto node = PushNewChild(data);
        node->type = AstNodeType_Enum;

        RegisterProcessedType(data, spellingStr, node);

        MakeChildrenVector(node);

        CXType clangUnderlyingType = clang_getEnumDeclIntegerType(cursor);
        BuiltInType buildInType = ClangTypeToBuiltIn(clangUnderlyingType.kind);

        auto nodeData = (EnumData *)malloc(sizeof(EnumData));
        nodeData->name = typeName;
        nodeData->underlyingType = buildInType;
        nodeData->anonymous = anonymous;
        node->data = nodeData;

        ExtractAttributes(cursor, data, &nodeData->attributes.attributes, &nodeData->attributes.attributesCount);

        clang_visitChildren(cursor, EnumVisitor, data);

        ConvertChildrenVectorToArray(node);

        PopNode(data);
    } break;

    case CXCursor_StructDecl:
    {
        if (!clang_isCursorDefinition(cursor))
        {
            break;
        }

        CXType type = clang_getCursorType(cursor);
        CXString spelling = clang_getTypeSpelling(type);
        const char *spellingStr = clang_getCString(spelling);
        bool anonymous = IsAnonymousType(spellingStr);
        const char *typeName = ExtractTypeName(spellingStr);

        if (CheckTypeIsAlreadyProcessed(data, typeName))
        {
            clang_disposeString(spelling);
            break;
        }

        auto node = PushNewChild(data);
        node->type = AstNodeType_Struct;

        RegisterProcessedType(data, typeName, node);

        MakeChildrenVector(node);

        auto nodeData = (StructData *)malloc(sizeof(StructData));
        nodeData->name = typeName;
        nodeData->size = (u32)clang_Type_getSizeOf(type);
        nodeData->align = (u32)clang_Type_getAlignOf(type);
        nodeData->anonymous = anonymous;

        node->data = nodeData;

        ExtractAttributes(cursor, data, &nodeData->attributes.attributes, &nodeData->attributes.attributesCount);

        clang_visitChildren(cursor, StructVisitor, data);

        ConvertChildrenVectorToArray(node);

        PopNode(data);
    } break;

    default: { printf("Error: Invalid type.\n"); } break;
    }
}

CXChildVisitResult StructVisitor(CXCursor cursor, CXCursor parent, CXClientData _data)
{
    auto data = (VisitorData*)_data;
    CXCursorKind kind = clang_getCursorKind(cursor);

    if (kind == CXCursor_FieldDecl)
    {
        AstNode* node = PushNewChild(data);
        node->type = AstNodeType_Field;
        auto nodeData = (FieldData *)malloc(sizeof(FieldData));
        node->data = nodeData;

        CXString name = clang_getCursorSpelling(cursor);
        const char* nameStr = clang_getCString(name);

        long long offset = clang_Cursor_getOffsetOfField(cursor);
        // TODO: Proper error handling.
        Assert(offset >= 0);

        CXType type = clang_getCursorType(cursor);

        TypeInfo* typeInfo = ResolveFieldType(type, data);

        ExtractAttributes(cursor, data, &nodeData->attributes.attributes, &nodeData->attributes.attributesCount);

        nodeData->name = nameStr;
        nodeData->offset = (u32)offset;
        nodeData->typeInfo = typeInfo;

        PopNode(data);
    }
    else if (kind == CXCursor_StructDecl || kind == CXCursor_EnumDecl)
    {
        TypeVisitor(cursor, data);
    }
    else if (kind != CXCursor_AnnotateAttr &&
             kind != CXCursor_StructDecl &&
             kind != CXCursor_EnumDecl)
    {
        LogPrint("Unexpected syntax in struct\n.");
    }


    return CXChildVisit_Continue;
}

bool CheckMetaprogramVisibility(CXCursor attribCursor)
{
    bool visible = false;
    static const char *metaprogramKeyword = "__metaprogram";
    static const int metaprogramKeywordSize = sizeof("__metaprogram");

    CXString attribSpelling = clang_getCursorSpelling(attribCursor);
    const char *attribSpellingStr = clang_getCString(attribSpelling);

    const char *keyword = strstr(attribSpellingStr, metaprogramKeyword);
    if (keyword)
    {
        keyword += metaprogramKeywordSize;
        if (strcmp(keyword, "\"metaprogram_visible\"") == 0)
        {
            visible = true;
        }
    }

    clang_disposeString(attribSpelling);

    return visible;
}

CXChildVisitResult AttributesVisitor(CXCursor cursor, CXCursor parent, CXClientData _data)
{
    auto data = (VisitorData*)_data;

    CXCursorKind kind = clang_getCursorKind(cursor);
    CXCursorKind parentKind = clang_getCursorKind(parent);

    if (kind == CXCursor_AnnotateAttr && CheckMetaprogramVisibility(cursor))
    {
        TypeVisitor(parent, data);
    }

    return CXChildVisit_Recurse;
}

#if defined(OS_WINDOWS)

#include "Windows.h"

bool CallMetaprogram(AstNode* root, const char* moduleName, const char* procName)
{
    HMODULE modulePtr = LoadLibraryA(moduleName);
    if (modulePtr == nullptr)
    {
        LogPrint("Failed to load metaprogram module %s\n", moduleName);
        return false;
    }

    auto userProc = (MetaprogramProc *)GetProcAddress(modulePtr, procName);
    if (userProc == nullptr)
    {
        LogPrint("Unable to find metaprogram procedure %s in module %s\n", procName, moduleName);
        return false;
    }

    LogPrint("Running metaprogram procedure %s from module %s ...\n", procName, moduleName);

    userProc(root);

    return true;
}
#endif

int main(int argc, char** argv)
{
    int compilerArgsCount = argc;
    char** compilerArgsPtr = nullptr;

    char* moduleName = nullptr;
    char* procName = nullptr;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-m") == 0 && i < argc - 1)
        {
            moduleName = argv[i + 1];
            i++;
        }

        if (strcmp(argv[i], "-p") == 0 && i < argc - 1)
        {
            procName = argv[i + 1];
            i++;
        }

        if (strcmp(argv[i], "-a") == 0)
        {
            compilerArgsCount = argc - (i + 1);
            compilerArgsPtr = argv + (i + 1);
            break;
        }
    }

    bool failed = false;

    if (moduleName == nullptr)
    {
        LogPrint("Metaprogram module is not specified! Use -m <module> to specify metaprogram module.\n");
        failed = true;
    }

    if (procName == nullptr)
    {
        LogPrint("Metaprogram procedure name is not specified! Use -p <name> to specify procedure name.\n");
        failed = true;
    }

    if (compilerArgsPtr == nullptr)
    {
        failed = true;
        LogPrint("Compiler flags are not specified! Use -a to specify compiler flags.\n");
    }

    if (failed)
    {
        return -1;
    }

    // NOTE: I didn't find any way to check if translation unit has errors.
    // So the solution is to redirect stderr to pipe and then read from that pipe
    // If it contains anything, then we have errors.

    // Opening pipe and redirecting stderr to it.
    int fd[2];
    // Create pipe
    _pipe(fd, 512 * 1024, O_BINARY);
    // Open file for reading from pipe
    FILE* readFile = _fdopen(fd[0], "rb");
    // Save current stderr descriptor
    int stderrCopy = _dup(_fileno(stderr));
    // Replace stderr with pipe
    _dup2(fd[1], _fileno(stderr));

    CXIndex index = clang_createIndex(0, 1);
    Assert(index);
    CXTranslationUnit unit = nullptr;
    CXErrorCode resultTr = clang_parseTranslationUnit2FullArgv(index, nullptr, compilerArgsPtr, compilerArgsCount, 0, 0, CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_DetailedPreprocessingRecord, &unit);

    // Redirect stderr back to where it was.
    // Close writing pipe. That way we will be able to correctly read from read file.
    _close(fd[1]);
    // Retstore stderr
    _dup2(stderrCopy, _fileno(stderr));
    _close(stderrCopy);

    bool hasErrors = false;

    // Dump pipe contents back to stderr and check if we have errors.
    char buffer[1024];
    while (!feof(readFile))
    {
        if (fgets(buffer, 1024, readFile) == NULL)
        {
            break;
        }

        fputs(buffer, stderr);
        hasErrors = true;
    }

    if (hasErrors)
    {
        // Set text color to red
        LogPrint("\033[0;31m");
        LogPrint("Translation unit contains errors. Metaprogram will not be executed.\n");
        // Reset text color
        LogPrint("\033[0m");
        return -1;
    }

    CXCursor rootCursor = clang_getTranslationUnitCursor(unit);

    VisitorData data;
    data.rootNode.type = AstNodeType_Root;
    MakeChildrenVector(&data.rootNode);

    data.stack.push_back(&data.rootNode);

    clang_visitChildren(rootCursor, AttributesVisitor, &data);

    ConvertChildrenVectorToArray(&data.rootNode);

    CallMetaprogram(&data.rootNode, moduleName, procName);

    //unsigned int level = 0;
    //clang_visitChildren(rootCursor, DumpAst, &level);

    return 0;
}

#if false
#include <string>
#include <iostream>

std::string getCursorKindName( CXCursorKind cursorKind )
{
  CXString kindName  = clang_getCursorKindSpelling( cursorKind );
  std::string result = clang_getCString( kindName );

  clang_disposeString( kindName );
  return result;
}

std::string getCursorSpelling( CXCursor cursor )
{
  CXString cursorSpelling = clang_getCursorSpelling( cursor );
  std::string result      = clang_getCString( cursorSpelling );

  clang_disposeString( cursorSpelling );
  return result;
}

CXChildVisitResult DumpAst(CXCursor cursor, CXCursor parent, CXClientData clientData)
{
    CXSourceLocation location = clang_getCursorLocation(cursor);
    if (clang_Location_isFromMainFile(location) == 0)
        return CXChildVisit_Continue;

    CXCursorKind cursorKind = clang_getCursorKind(cursor);

    unsigned int curLevel = *(reinterpret_cast<unsigned int *>(clientData));
    unsigned int nextLevel = curLevel + 1;

    std::cout << std::string(curLevel, '-') << " " << getCursorKindName(cursorKind) << " (" << getCursorSpelling(cursor) << ")\n";

    clang_visitChildren(cursor,
                        DumpAst,
                        &nextLevel);

    return CXChildVisit_Continue;
}
#endif
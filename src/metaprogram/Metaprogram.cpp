#define _CRT_SECURE_NO_WARNINGS
#include "AST.h"

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <string.h>

#include <vector>
#include <string>
#include <unordered_map>

#include "../../ext/meow_hash/meow_hash_x64_aesni.h"

char TemporaryStringBuffer[2048];

const char* GetResolvedTypeName(AstNode* node);

typedef void(OutputProc)(void* data, const char* fmt, ...);

void SerializeType(TypeInfo* info, OutputProc outputProc, void* userData)
{
    if (!info)
    {
        return;
    }

    switch (info->kind)
    {
        case TypeKind_Unknown:
        {
            outputProc(userData, " <unknown>");
        } break;

        case TypeKind_BuiltIn:
        {
            outputProc(userData, " %s", BuiltInType_Strings[info->builtInType]);
        } break;

        case TypeKind_Pointer:
        {
            outputProc(userData, " *");
            SerializeType(info->underlyingType, outputProc, userData);
        } break;

        case TypeKind_FunctionProto:
        {
            outputProc(userData, " FunctionProto");
        } break;

        case TypeKind_Array:
        {
            if (info->arrayHasSize)
            {
                outputProc(userData, " [%lu]", info->arrayCount);
            }
            else
            {
                outputProc(userData, " []");
            }

            SerializeType(info->underlyingType, outputProc, userData);
        } break;

        case TypeKind_Struct:
        {
            outputProc(userData, " %s", GetResolvedTypeName(info->resolvedTypeNode));
        } break;

        case TypeKind_Enum:
        {
            outputProc(userData, " %s", GetResolvedTypeName(info->resolvedTypeNode));
        } break;

        case TypeKind_Unresolved:
        {
            outputProc(userData, " %s", info->unresolvedTypeName);
        } break;
    }
}

void SerializeAst(AstNode* node, u32 level, OutputProc outputProc, void* userData)
{
    if (node->type != AstNodeType_Root)
    {
        outputProc(userData, "\n");
    }

    for (u32 i = 0; i < level; i++)
    {
        outputProc(userData, "%c", '-');
    }

    if (level == 0)
    {
        outputProc(userData, "%s", AstNodeType_Strings[node->type]);
    }
    else
    {
        outputProc(userData, " %s", AstNodeType_Strings[node->type]);
    }

    switch (node->type)
    {
        case AstNodeType_Enum:
        {
            auto data = (EnumData*)node->data;
            outputProc(userData, " %s %s anonymous: %s", data->name, BuiltInType_Strings[data->underlyingType], data->anonymous ? "true" : "false");
        } break;

        case AstNodeType_EnumConstant:
        {
            auto data = (EnumConstantData*)node->data;
            outputProc(userData, " %s sValue: %ld uValue: %lu", data->name, data->signedValue, data->unsignedValue);
        } break;

        case AstNodeType_Struct:
        {
            auto data = (StructData*)node->data;
            outputProc(userData, " %s size: %lu align: %lu anonymous: %s", data->name, data->size, data->align, data->anonymous ? "true" : "false");
        } break;

        case AstNodeType_Field:
        {
            auto data = (FieldData*)node->data;
            outputProc(userData, " %s offset: %lu, type:", data->name, data->offset);
            SerializeType(data->typeInfo, outputProc, userData);
        } break;
    }

    for (u32 i = 0; i < node->childrenCount; i++)
    {
        SerializeAst(node->children[i], level + 1, outputProc, userData);
    }
}

void SerializeAst(AstNode* node, OutputProc outputProc, void* userData)
{
    SerializeAst(node, 0, outputProc, userData);
}

void AstOutputProc(void*, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

struct MetadataGeneratorContext
{
    OutputProc* srcOutputProc;
    void* srcOutputProcData;

    OutputProc* headerOutputProc;
    void *headerOutputProcData;

    u32 currentTypeId;
    std::vector<AstNode*> stack;
    std::vector<std::string> initializers;
    std::unordered_map<std::string, u32> typeIdTable;
};

void GenerateAnonymousEnumName(const char* astNameString, char* buffer)
{
    u32 hash = MeowU32From(MeowHash(MeowDefaultSeed, strlen(astNameString), (void *)astNameString), 0);
    sprintf(buffer, "AnonymousEnum_%#x", hash);
}

void GenerateAnonymousStructName(const char* astNameString, char* buffer)
{
    u32 hash = MeowU32From(MeowHash(MeowDefaultSeed, strlen(astNameString), (void *)astNameString), 0);
    sprintf(buffer, "AnonymousStruct_%#x", hash);
}

const char* GetResolvedTypeName(AstNode* node)
{
    switch (node->type)
    {
        case AstNodeType_Enum:
        {
            auto data = (EnumData*)node->data;
            if (data->anonymous)
            {
                GenerateAnonymousEnumName(data->name, TemporaryStringBuffer);
                return TemporaryStringBuffer;
            }
            else
            {
                return data->name;
            }
        }

        case AstNodeType_Struct:
        {
            auto data = (StructData*)node->data;
            if (data->anonymous)
            {
                GenerateAnonymousStructName(data->name, TemporaryStringBuffer);
                return TemporaryStringBuffer;
            }
            else
            {
                return data->name;
            }
        }

        InvalidDefault();
    }

    return nullptr;
}

// TODO: Better attributes handling
bool HasAttribute(const AttributesList* list, const char* attrib)
{
    #if false
    bool result = false;

    for (u32 i = 0; i < list->attributesCount; i++)
    {
        if (strcmp(list->attributes[i].attributeString, attrib) == 0)
        {
            result = true;
            break;
        }
    }

    return result;
#endif
return false;
}

void GenerateEnumMetadataSource(AstNode* node, MetadataGeneratorContext* context)
{
    assert(node->type == AstNodeType_Enum);
    context->stack.push_back(node);
    auto data = (EnumData*)node->data;
    const char* name = data->name;

    char nameBuffer[64];
    if (data->anonymous)
    {
        GenerateAnonymousEnumName(data->name, nameBuffer);
        name = nameBuffer;
    }

    u32 typeId = context->typeIdTable[std::string(name)];

        //for (u32 i = 0; i < data->attributesCount; i++)
        //{
        //    printf("Enum Attribute: %s\n", data->attributes[i].attributeString);
        //}

    context->headerOutputProc(context->headerOutputProcData, "#define _TypeInfo_TypeId_%s TypeId_Create(%lu)\n", name, typeId);
    context->headerOutputProc(context->headerOutputProcData, "#define _EnumMetadata_ConstantsCount_%s %lu\n", name, node->childrenCount);
    context->headerOutputProc(context->headerOutputProcData, "#define _EnumMetadata_Name_%s \"%s\"\n", name, name);
    context->headerOutputProc(context->headerOutputProcData, "extern EnumMetadata _EnumMetadata_%s;\n", name);
    context->headerOutputProc(context->headerOutputProcData, "\n");

    context->srcOutputProc(context->srcOutputProcData, "static EnumMetadata _EnumMetadata_%s;\n", name);
    context->srcOutputProc(context->srcOutputProcData, "\n");
    context->srcOutputProc(context->srcOutputProcData, "void _EnumMetadata_Init_%s(Allocator* allocator)\n", name);
    context->srcOutputProc(context->srcOutputProcData, "{\n");
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.typeId = TypeId_Create(%lu);\n", name, typeId);
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.name = \"%s\";\n", name, name);
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.constantsCount = %lu;\n", name, node->childrenCount);
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.isFlags = %s;\n", name, HasAttribute(&data->attributes, "Flags") ? "true" : "false");
    context->srcOutputProc(context->srcOutputProcData, "\n");
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.constants = Allocator_AllocZero(allocator, sizeof(EnumConstantData) * %lu);\n", name, node->childrenCount);
    context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.indexTable = HashMap_EnumIndex_Create(allocator);\n", name);
    context->srcOutputProc(context->srcOutputProcData, "\n");

    for (u32 i = 0; i < node->childrenCount; i++)
    {
        auto child = node->children[i];
        assert(child->type == AstNodeType_EnumConstant);
        auto childData = (EnumConstantData*)child->data;
        context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.constants[%lu].name = \"%s\";\n", name, i, childData->name);
        context->srcOutputProc(context->srcOutputProcData, "    _EnumMetadata_%s.constants[%lu].value = (EnumValueType)%s;\n", name, i, childData->name);
        context->srcOutputProc(context->srcOutputProcData, "    *HashMap_EnumIndex_Add(&_EnumMetadata_%s.indexTable, (EnumValueType)%s) = %lu;\n", name, childData->name, i);
        context->srcOutputProc(context->srcOutputProcData, "\n");

#if 0
        for (u32 i = 0; i < childData->attributesCount; i++)
        {
            printf("Enum Attribute: %s\n", childData->attributes[i].attributeString);
        }
#endif
    }

    context->srcOutputProc(context->srcOutputProcData, "}\n");
    context->srcOutputProc(context->srcOutputProcData, "\n");

    context->initializers.emplace_back();

    sprintf(TemporaryStringBuffer, "    _EnumMetadata_Init_%s(allocator);\n", name);
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].kind = %s;\n", typeId, "TypeMetadataKind_Enum");
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].data = &_EnumMetadata_%s;\n", typeId, name);
    context->initializers.back().append(TemporaryStringBuffer);
}

u32 CountStructFields(AstNode* node)
{
    assert(node->type == AstNodeType_Struct);

    u32 count = 0;
    for (u32 i = 0; i < node->childrenCount; i++)
    {
        if (node->children[i]->type == AstNodeType_Field)
        {
            count++;
        }
    }

    return count;
}

bool ResolveFieldAndOutputSource(TypeInfo* typeInfo, MetadataGeneratorContext* context, u32 index, const char* structName, FieldData* fieldData)
{
    if (typeInfo->kind == TypeKind_Array)
    {
        if (!typeInfo->arrayHasSize)
        {
            // TODO: Better error handling all over here.
            // TODO: Crash on error
            printf("Reflection for arrays of unknown size is not supported!\n");
            return false;
        }

        assert(typeInfo->underlyingType);
        auto arrayType = typeInfo->underlyingType;

        if (ResolveFieldAndOutputSource(arrayType, context, index, structName, fieldData))
        {
            context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields[%lu].isArray = true;\n", structName, index);
            context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields[%lu].arrayCount = %lu;\n", structName, index, typeInfo->arrayCount);
        }

        return true;
    }

    if (typeInfo->kind == TypeKind_Pointer)
    {
        if (HasAttribute(&fieldData->attributes, "Array"))
        {
            printf("Array Pointer\n");
        }
        return false;
    }

    u32 fieldTypeId;

    if (typeInfo->kind == TypeKind_BuiltIn)
    {
        fieldTypeId = context->typeIdTable[BuiltInType_Strings[(u32)typeInfo->builtInType]];
    }
    else if (typeInfo->kind == TypeKind_Struct || typeInfo->kind == TypeKind_Enum)
    {
        // TODO: What if we didn't find a type.
        fieldTypeId = context->typeIdTable[GetResolvedTypeName(typeInfo->resolvedTypeNode)];
    }
    else
    {
        printf("Reflection of type %s is not supported yet!\n", TypeKind_Strings[(u32)typeInfo->kind]);
        return false;
    }

    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields[%lu].name = \"%s\";\n", structName, index, fieldData->name);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields[%lu].typeId = TypeId_Create(%lu);\n", structName, index, fieldTypeId);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields[%lu].offset = %lu;\n", structName, index, fieldData->offset);

    return true;
}

void GenerateStructMetadataSource(AstNode* node, MetadataGeneratorContext* context)
{
    assert(node->type == AstNodeType_Struct);
    context->stack.push_back(node);

    auto data = (StructData*)node->data;
    const char* name = data->name;

    char nameBuffer[64];
    if (data->anonymous)
    {
        GenerateAnonymousStructName(data->name, nameBuffer);
        name = nameBuffer;
    }

    u32 typeId = context->typeIdTable[std::string(name)];
    u32 fieldsCount = CountStructFields(node);

    context->headerOutputProc(context->headerOutputProcData, "#define _TypeInfo_TypeId_%s TypeId_Create(%lu)\n", name, typeId);
    context->headerOutputProc(context->headerOutputProcData, "#define _StructMetadata_Name_%s \"%s\"\n", name, name);
    context->headerOutputProc(context->headerOutputProcData, "extern StructMetadata _StructMetadata_%s;\n", name);

    context->srcOutputProc(context->srcOutputProcData, "static StructMetadata _StructMetadata_%s;\n", name);
    context->srcOutputProc(context->srcOutputProcData, "\n");
    context->srcOutputProc(context->srcOutputProcData, "void _StructMetadata_Init_%s(Allocator* allocator)\n", name);
    context->srcOutputProc(context->srcOutputProcData, "{\n");
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.typeId = TypeId_Create(%lu);\n", name, typeId);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.name = \"%s\";\n", name, name);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.size = %lu;\n", name, data->size);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.align = %lu;\n", name, data->align);
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fieldsCount = %lu;\n", name, fieldsCount);
    context->srcOutputProc(context->srcOutputProcData, "\n");
    context->srcOutputProc(context->srcOutputProcData, "    _StructMetadata_%s.fields = Allocator_AllocZero(allocator, sizeof(StructFieldMetadata) * %lu);\n", name, fieldsCount);
    context->srcOutputProc(context->srcOutputProcData, "\n");

    u32 count = 0;
    for (u32 i = 0; i < node->childrenCount; i++)
    {
        auto child = node->children[i];
        if (child->type != AstNodeType_Field) continue;
        auto childData = (FieldData*)child->data;
        count++;

        ResolveFieldAndOutputSource(childData->typeInfo, context, count - 1, name, childData);
    }

    context->srcOutputProc(context->srcOutputProcData, "}\n");
    context->srcOutputProc(context->srcOutputProcData, "\n");

    context->initializers.emplace_back();

    sprintf(TemporaryStringBuffer, "    _StructMetadata_Init_%s(allocator);\n", name);
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].kind = %s;\n", typeId, "TypeMetadataKind_Struct");
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].data = &_StructMetadata_%s;\n", typeId, name);
    context->initializers.back().append(TemporaryStringBuffer);
}

void GenerateBuiltInTypeMetadataSource(MetadataGeneratorContext* context, BuiltInType type, const char* spelling)
{
    context->stack.push_back(nullptr);

    const char* name = BuiltInType_Strings[(u32)type];
    u32 typeId = context->typeIdTable[std::string(name)];

    context->headerOutputProc(context->headerOutputProcData, "#define _TypeInfo_TypeId_%s TypeId_Create(%lu)\n", name, typeId);
    context->headerOutputProc(context->headerOutputProcData, "#define _BuiltInTypeMetadata_Name_%s \"%s\"\n", name, name);
    context->headerOutputProc(context->headerOutputProcData, "extern BuiltInTypeMetadata _BuiltInTypeMetadata_%s;\n", name);
    context->headerOutputProc(context->headerOutputProcData, "\n");

    context->srcOutputProc(context->srcOutputProcData, "static BuiltInTypeMetadata _BuiltInTypeMetadata_%s;\n", name);
    context->srcOutputProc(context->srcOutputProcData, "\n");
    context->srcOutputProc(context->srcOutputProcData, "void _BuiltInTypeMetadata_Init_%s()\n", name);
    context->srcOutputProc(context->srcOutputProcData, "{\n");
    context->srcOutputProc(context->srcOutputProcData, "    _BuiltInTypeMetadata_%s.typeId = TypeId_Create(%lu);\n", name, typeId);
    context->srcOutputProc(context->srcOutputProcData, "    _BuiltInTypeMetadata_%s.name = \"%s\";\n", name, name);
    context->srcOutputProc(context->srcOutputProcData, "    _BuiltInTypeMetadata_%s.size = sizeof(%s);\n", name, spelling);
    context->srcOutputProc(context->srcOutputProcData, "    _BuiltInTypeMetadata_%s.align = _Alignof(%s);\n", name, spelling);
    context->srcOutputProc(context->srcOutputProcData, "\n");

    context->srcOutputProc(context->srcOutputProcData, "}\n");
    context->srcOutputProc(context->srcOutputProcData, "\n");

    context->initializers.emplace_back();

    sprintf(TemporaryStringBuffer, "    _BuiltInTypeMetadata_Init_%s();\n", name);
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].kind = %s;\n", typeId, "TypeMetadataKind_BuiltIn");
    context->initializers.back().append(TemporaryStringBuffer);
    sprintf(TemporaryStringBuffer, "    __TypeInfo_TypeMetadata[%lu].data = &_BuiltInTypeMetadata_%s;\n", typeId, name);
    context->initializers.back().append(TemporaryStringBuffer);
}

void GenerateMetadataSourceRecursive(AstNode* node, MetadataGeneratorContext* context)
{
    if (node->type == AstNodeType_Enum)
    {
        GenerateEnumMetadataSource(node, context);
    }
    else if (node->type == AstNodeType_Struct)
    {
        GenerateStructMetadataSource(node, context);
    }

    for (u32 i = 0; i < node->childrenCount; i++)
    {
        GenerateMetadataSourceRecursive(node->children[i], context);
    }
}

void FillTypeIdTable(AstNode* node, MetadataGeneratorContext* context)
{
    if (node->type == AstNodeType_Enum)
    {
        auto data = (EnumData* )node->data;
        const char* name;
        if (data->anonymous)
        {
            TemporaryStringBuffer[0] = 0;
            GenerateAnonymousEnumName(data->name, TemporaryStringBuffer);
            name = TemporaryStringBuffer;
        }
        else
        {
            name = data->name;
        }

        context->typeIdTable[std::string(name)] = ++context->currentTypeId;
    }
    else if (node->type == AstNodeType_Struct)
    {
        auto data = (StructData* )node->data;
        const char* name;
        if (data->anonymous)
        {
            TemporaryStringBuffer[0] = 0;
            GenerateAnonymousStructName(data->name, TemporaryStringBuffer);
            name = TemporaryStringBuffer;
        }
        else
        {
            name = data->name;
        }

        context->typeIdTable[std::string(name)] = ++context->currentTypeId;
    }

    for (u32 i = 0; i < node->childrenCount; i++)
    {
        FillTypeIdTable(node->children[i], context);
    }
}

void GenerateMetadataSource(AstNode* node, OutputProc* srcOutputProc, void* srcOutputProcData, OutputProc* headerOutputProc, void* headerOutputProcData)
{
    MetadataGeneratorContext context;
    context.srcOutputProc = srcOutputProc;
    context.srcOutputProcData = srcOutputProcData;
    context.headerOutputProc = headerOutputProc;
    context.headerOutputProcData = headerOutputProcData;

    context.currentTypeId = 0;

    // Skip unknown
    for (int i = 1; i < ArrayCount(BuiltInType_Strings); i++)
    {
        context.typeIdTable[std::string(BuiltInType_Strings[i])] = ++context.currentTypeId;
    }

    //FillTypeIdTable(node, &context);

#if 0
    for (auto it : context.typeIdTable)
    {
        printf("- %s : %+lu\n", it.first.c_str(), it.second);
    }
#endif

    headerOutputProc(headerOutputProcData, "#pragma once\n");

    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_SignedShort, "signed short");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_UnsignedShort, "unsigned short");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_SignedInt, "signed int");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_UnsignedInt, "unsigned int");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_SignedLong, "signed long");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_UnsignedLong, "unsigned long");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_SignedLongLong, "signed long long");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_UnsignedLongLong, "unsigned long long");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_SignedChar, "signed char");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_UnsignedChar, "unsigned char");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Char, "char");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Bool, "bool");
    //GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Char8, "char8_t");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Char16, "char16_t");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Char32, "char32_t");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Wchar, "wchar_t");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Float, "float");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_Double, "double");
    GenerateBuiltInTypeMetadataSource(&context, BuiltInType_LongDouble, "long double");

    GenerateMetadataSourceRecursive(node, &context);

    srcOutputProc(srcOutputProcData, "static const u32 _TypeInfo_TypeMetadataCount = %lu;\n", context.currentTypeId + 1);
    srcOutputProc(srcOutputProcData, "static TypeMetadata __TypeInfo_TypeMetadata[%lu];\n", context.currentTypeId + 1);
    srcOutputProc(srcOutputProcData, "static const TypeMetadata* _TypeInfo_TypeMetadata = __TypeInfo_TypeMetadata;\n");
    srcOutputProc(srcOutputProcData, "\n");

    srcOutputProc(srcOutputProcData, "void Reflection_Init(Allocator* allocator)\n");
    srcOutputProc(srcOutputProcData, "{\n");
    srcOutputProc(srcOutputProcData, "    memset(__TypeInfo_TypeMetadata, 0, sizeof(TypeMetadata));\n");
    srcOutputProc(srcOutputProcData, "\n");


    u32 typeIndex = 1;
    for (auto initializer : context.initializers)
    {
        srcOutputProc(srcOutputProcData, "%s", initializer.c_str());
        srcOutputProc(srcOutputProcData, "\n");
    }

    srcOutputProc(srcOutputProcData, "}\n");
}

void OutputToFileProc(void* file, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vfprintf((FILE*)file, fmt, args);
    va_end(args);
}

extern "C" void Metaprogram(AstNode* astRoot)
{
    FILE* srcFile = fopen("src/Reflection.Generated.c", "w");
    FILE* headerFile = fopen("src/Reflection.Generated.h", "w");
    GenerateMetadataSource(astRoot, OutputToFileProc, srcFile, OutputToFileProc, headerFile);
    fclose(srcFile);
    fclose(headerFile);
    //SerializeAst(astRoot, AstOutputProc, nullptr);
}

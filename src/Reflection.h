#pragma once

#include "core/Common.h"

typedef u32 EnumValueType;

typedef struct
{
    u32 id;
} TypeId;

TypeId TypeId_Create(u32 id)
{
    TypeId typeId;
    typeId.id = id;
    return typeId;
}

#define TKey EnumValueType
#define TValue u32
#define TypeName EnumIndex
// Not sure if this is a good idea to use meow hash for hash tables.
#define GetHash(x) (MeowU32From(MeowHash(MeowDefaultSeed, sizeof((x)), &(x)), 0))
#define CompareKeys(a, b) (a == b)

#include "HashMap.h"
#define HashMap_EnumIndex_ForEach(map) _defineHashMapForeach(EnumIndex, map)

#undef TKey
#undef TValue
#undef TypeName
#undef GetHash
#undef CompareKeys

#if !defined(META_PASS)

#define TypeInfo_GetTypeId(type) _TypeInfo_TypeId_##type
#define Enum_GetConstantsInfo(type) ((const EnumConstantData*)(_EnumMetadata_##type .constants))
#define Enum_GetCount(type) _EnumMetadata_ConstantsCount_##type
#define Enum_GetName(type) _EnumMetadata_Name_##type
#define Enum_GetConstantName(type, x) EnumMetadata_GetConstantName(&_EnumMetadata_##type, x)
#define Enum_GetConstantInfo(type, x) EnumMetadata_GetConstantInfo(&_EnumMetadata_##type, x)

#define Struct_GetName(type) _StructMetadata_Name_##type
#define Struct_GetName(type) _StructMetadata_Name_##type

#else

#define TypeInfo_GetTypeId(type) 0
#define Enum_GetConstantsInfo(type) nullptr
#define Enum_GetCount(type) 0
#define Enum_GetName(type) ""
#define Enum_GetConstantName(type, x) ""
#define Enum_GetConstantInfo(type, x) nullptr

#endif

void Reflection_Init(Allocator* allocator);

typedef struct
{
    const char* name;
    EnumValueType value;
} EnumConstantData;

typedef struct
{
    const char* name;
    TypeId typeId;
    u32 offset;
    bool isArray;
    u32 arrayCount;
} StructFieldMetadata;

typedef enum
{
    TypeMetadataKind_Invalid = 0,
    TypeMetadataKind_BuiltIn,
    TypeMetadataKind_Enum,
    TypeMetadataKind_Struct
} TypeMetadataKind;

typedef struct
{
    TypeMetadataKind kind;
    void* data;
} TypeMetadata;

typedef struct
{
    TypeId typeId;
    const char* name;
    bool isFlags;
    u32 constantsCount;
    EnumConstantData* constants;
    HashMap_EnumIndex indexTable;
} EnumMetadata;

typedef struct
{
    TypeId typeId;
    const char* name;
    u32 size;
    u32 align;
    u32 fieldsCount;
    StructFieldMetadata* fields;
} StructMetadata;

typedef struct
{
    TypeId typeId;
    const char* name;
    u32 size;
    u32 align;
} BuiltInTypeMetadata;

extern const u32 _TypeInfo_TypeMetadataCount;
extern const TypeMetadata* _TypeInfo_TypeMetadata;

const TypeMetadata* TypeInfo_GetTypeInfo(TypeId id)
{
    const TypeMetadata* result = nullptr;

    if (id.id < _TypeInfo_TypeMetadataCount)
    {
        result = _TypeInfo_TypeMetadata + id.id;
    }

    return result;
}

const char* TypeInfo_GetTypeName(TypeId id)
{
    const TypeMetadata* typeData = TypeInfo_GetTypeInfo(id);

    if (typeData)
    {
        switch (typeData->kind)
        {
            case TypeMetadataKind_Enum: return ((EnumMetadata*)typeData->data)->name;
            case TypeMetadataKind_Struct: return ((StructMetadata*)typeData->data)->name;
            case TypeMetadataKind_BuiltIn: return ((BuiltInTypeMetadata*)typeData->data)->name;
            default: return "<unknown>";
        }
    }

    return "<unknown>";
}

const StructMetadata* TypeInfo_GetStructInfo(TypeId id)
{
    StructMetadata* data = nullptr;

    const TypeMetadata* typeData = TypeInfo_GetTypeInfo(id);
    if (typeData != nullptr && typeData->kind == TypeMetadataKind_Struct)
    {
        data = (StructMetadata*)typeData->data;
    }

    return data;
}

const EnumMetadata* TypeInfo_GetEnumInfo(TypeId id)
{
    EnumMetadata* enumData = nullptr;

    const TypeMetadata* typeData = TypeInfo_GetTypeInfo(id);
    if (typeData != nullptr && typeData->kind == TypeMetadataKind_Enum)
    {
        enumData = (EnumMetadata*)typeData->data;
    }

    return enumData;
}

const char* EnumMetadata_GetConstantName(EnumMetadata* data, EnumValueType x)
{
    const char* result = "<unknown>";

    u32* index =  HashMap_EnumIndex_Find(&(data->indexTable), x);
    if (index != nullptr)
    {
        result = data->constants[*index].name;
    }

    return result;
}

const EnumConstantData* EnumMetadata_GetConstantInfo(EnumMetadata* data, EnumValueType x)
{
    const EnumConstantData* result = nullptr;

    u32* index =  HashMap_EnumIndex_Find(&(data->indexTable), x);
    if (index != nullptr)
    {
        result = data->constants + (*index);
    }

    return result;
}

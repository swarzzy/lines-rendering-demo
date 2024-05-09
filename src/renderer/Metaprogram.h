#pragma once

#include <vector>
#include <string>

#include <stdio.h>
#include <assert.h>
#include <string.h>

bool ValidateType(TypeInfo* info)
{
    return info->kind == TypeKind_Pointer && info->underlyingType->kind == TypeKind_FunctionProto;
}

void VisitAst(AstNode* node, std::string& buffer)
{
    if (node->type == AstNodeType_Struct)
    {
        auto data = (StructData*)node->data;
        for (u32 i = 0; i < data->attributes.attributesCount; i++)
        {
            if (strcmp(data->attributes.attributes[i].attributeName, "RendererAPI") == 0)
            {
                for (u32 i = 0; i < node->childrenCount; i++)
                {
                    auto child = node->children[i];
                    if (child->type == AstNodeType_Field)
                    {
                        auto data = (FieldData*)child->data;
                        if (ValidateType(data->typeInfo))
                        {
                            buffer = buffer + "api->" + data->name + " = " + data->name + ";\n\t";
                        }
                        else
                        {
                            buffer = buffer + "#error Error: RendererAPI field " + data->name + " is not a function pointer!\n\t";
                        }
                    }
                }
            }
        }
    }

    for (u32 i = 0; i < node->childrenCount; i++)
    {
        VisitAst(node->children[i], buffer);
    }
}

char* ReadEntireTextFile(const char* filename)
{
    char *fcontent = NULL;
    int fsize = 0;
    FILE *fp;

    fp = fopen(filename, "r");
    if(fp) {
        fseek(fp, 0, SEEK_END);
        fsize = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        fcontent = (char*) malloc(fsize + 1);
        memset(fcontent, 0, fsize + 1);
        fread(fcontent, 1, fsize, fp);

        fclose(fp);
    }

    return fcontent;
}

std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}

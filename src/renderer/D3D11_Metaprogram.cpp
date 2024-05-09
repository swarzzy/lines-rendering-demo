#define _CRT_SECURE_NO_WARNINGS

#include <AST.h>
#include "Metaprogram.h"

const char* TemplatePath = "src/renderer/RendererBindings.Template.c";
const char* OutFilePath = "src/renderer/D3D11_RendererBindings.Generated.c";

extern "C" void Metaprogram(AstNode* astRoot)
{
    auto templateText = std::string(ReadEntireTextFile(TemplatePath));
    FILE* destFile = fopen(OutFilePath, "w");

    if (destFile == nullptr)
    {
        printf("Failed to open %s\n", OutFilePath);
    }

    std::string buffer;
    VisitAst(astRoot, buffer);

    auto result = ReplaceString(templateText, "{{BINDINGS}}", buffer);

    fprintf(destFile, "%s", result.c_str());
    fclose(destFile);
}

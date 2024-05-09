#if !defined(META_PASS)

void InitializeApi(RendererAPI* api)
{
    api->SetDisplayParams = SetDisplayParams;
	api->SetVsyncMode = SetVsyncMode;
	api->SwapScreenBuffers = SwapScreenBuffers;
	api->SetViewport = SetViewport;
	api->ExecuteCommandBuffer = ExecuteCommandBuffer;
	api->LoadTexture2D = LoadTexture2D;
	api->CreateSampler = CreateSampler;
	api->UnloadTexture2D = UnloadTexture2D;
	api->BeginFrame = BeginFrame;
	api->EndFrame = EndFrame;
	
}

#endif

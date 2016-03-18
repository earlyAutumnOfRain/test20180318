// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanUniformBuffer.cpp: Vulkan Constant buffer implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"

const int NUM_SAFE_FRAMES = 5;
static TArray<TRefCountPtr<FVulkanBuffer>> GUBPool[NUM_SAFE_FRAMES];

static TRefCountPtr<FVulkanBuffer> AllocateBufferFromPool(FVulkanDevice& Device, uint32 ConstantBufferSize, EUniformBufferUsage Usage)
{
	if (Usage == UniformBuffer_MultiFrame)
	{
		return new FVulkanBuffer(Device, ConstantBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, __FILE__, __LINE__);
	}

	int32 BufferIndex = GFrameNumberRenderThread % NUM_SAFE_FRAMES;

	GUBPool[BufferIndex].Add(new FVulkanBuffer(Device, ConstantBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, __FILE__, __LINE__));
	return GUBPool[BufferIndex].Last();
}

void CleanupUniformBufferPool()
{
	int32 BufferIndex = (GFrameNumberRenderThread  + 1) % NUM_SAFE_FRAMES;
	GUBPool[BufferIndex].Reset(0);
}

/*-----------------------------------------------------------------------------
	Uniform buffer RHI object
-----------------------------------------------------------------------------*/

FVulkanUniformBuffer::FVulkanUniformBuffer(FVulkanDevice& Device, const FRHIUniformBufferLayout& InLayout, const void* Contents, EUniformBufferUsage Usage) :
	FRHIUniformBuffer(InLayout)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanUniformBufferCreateTime);

	// Verify the correctness of our thought pattern how the resources are delivered
	//	- If ResourceOffset has an offset, we also have at least one resource
	//	- If we have at least one resource, we also expect ResourceOffset to have an offset
	//	- Meaning, there is always a uniform buffer with a size specified larged than 0 bytes
	check(InLayout.Resources.Num() > 0 || InLayout.ConstantBufferSize > 0);
	check(Contents);

	if (InLayout.ConstantBufferSize)
	{
		static auto* CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Vulkan.UseRealUBs"));
		if (CVar && CVar->GetValueOnAnyThread() != 0)
		{
			Buffer = AllocateBufferFromPool(Device, InLayout.ConstantBufferSize, Usage);

			void* Data = Buffer->Lock(InLayout.ConstantBufferSize);
			FMemory::Memcpy(Data, Contents, InLayout.ConstantBufferSize);
			Buffer->Unlock();
		}
		else
		{
			// Create uniform buffer, which is stored on the CPU, the buffer is uploaded to a correct GPU
			// buffer in 'FVulkanBoundShaderState::UpdateDescriptorSets()'.
			ConstantData.AddUninitialized(InLayout.ConstantBufferSize);
			if (Contents)
			{
				FMemory::Memcpy(ConstantData.GetData(), Contents, InLayout.ConstantBufferSize);
			}
		}
	}

	// Parse Sampler and Texture resrouces, if necessary
	const uint32 NumResources = InLayout.Resources.Num();
	if(NumResources == 0)
	{
		return;
	}

	// Transfer the resource table to an internal resource-array
	FRHIResource** InResources = (FRHIResource**)((uint8*)Contents + InLayout.ResourceOffset);
	ResourceTable.Empty(NumResources);
	ResourceTable.AddZeroed(NumResources);
	for(uint32 Index = 0; Index < NumResources; Index++)
	{
		FRHIResource* CurrResource = InResources[Index];
		check(CurrResource);
		ResourceTable[Index] = CurrResource;
	}
}

FVulkanUniformBuffer::~FVulkanUniformBuffer()
{
}

FUniformBufferRHIRef FVulkanDynamicRHI::RHICreateUniformBuffer(const void* Contents, const FRHIUniformBufferLayout& Layout, EUniformBufferUsage Usage)
{
	SCOPE_CYCLE_COUNTER(STAT_VulkanCreateUniformBufferTime);
	check(IsInRenderingThread());

	// Emulation: Creates and returns a CPU-Only buffer.
	// Parts of the buffer are later on copied for each shader stage into the packed uniform buffer
	return new FVulkanUniformBuffer(*Device, Layout, Contents, Usage);
}

FVulkanPooledUniformBuffer::FVulkanPooledUniformBuffer(FVulkanDevice& InDevice, uint32 InSize)
    : Buffer(InDevice, InSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, false, __FILE__, __LINE__)
{
}

FVulkanPooledUniformBuffer::~FVulkanPooledUniformBuffer()
{

}

FVulkanGlobalUniformPool::FVulkanGlobalUniformPool()
{
}

FVulkanGlobalUniformPool::~FVulkanGlobalUniformPool()
{
    for (uint32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
    {
        GlobalUniformBufferPool[BucketIndex].Empty();
    }

    const uint32 NumUsedBuckets = NumPoolBuckets * NumFrames;
    for (uint32 UsedBucketIndex = 0; UsedBucketIndex < NumUsedBuckets; UsedBucketIndex++)
    {
        UsedGlobalUniformBuffers[UsedBucketIndex].Empty();
    }
}

static FORCEINLINE uint32 GetPoolBucketSize(uint32 NumBytes)
{
    return FMath::RoundUpToPowerOfTwo(NumBytes);
}

void FVulkanGlobalUniformPool::BeginFrame()
{
    const uint32 CurrentFrameIndex = GFrameNumberRenderThread % NumFrames;

    for (uint32 BucketIndex = 0; BucketIndex < NumPoolBuckets; BucketIndex++)
    {
        const uint32 UsedBucketIndex = CurrentFrameIndex * NumPoolBuckets + BucketIndex;

        GlobalUniformBufferPool[BucketIndex].Append(UsedGlobalUniformBuffers[UsedBucketIndex]);
        UsedGlobalUniformBuffers[UsedBucketIndex].Reset();
    }
}

FPooledUniformBufferRef& FVulkanGlobalUniformPool::GetGlobalUniformBufferFromPool(FVulkanDevice& InDevice, uint32 InSize)
{
    const uint32 BucketIndex = GetPoolBucketIndex(InSize);
    TArray<FPooledUniformBufferRef>& PoolBucket = GlobalUniformBufferPool[BucketIndex];

    const uint32 BufferSize = GetPoolBucketSize(InSize);

    int32 NewIndex = 0;
    const uint32 CurrentFrameIndex = GFrameNumberRenderThread % NumFrames;
    const uint32 UsedBucketIndex = CurrentFrameIndex * NumPoolBuckets + BucketIndex;
    if (PoolBucket.Num() > 0)
    {
        NewIndex = UsedGlobalUniformBuffers[UsedBucketIndex].Add(PoolBucket.Pop());
    }
    else
    {
        NewIndex = UsedGlobalUniformBuffers[UsedBucketIndex].Add(new FVulkanPooledUniformBuffer(InDevice, BufferSize));
    }

    return UsedGlobalUniformBuffers[UsedBucketIndex][NewIndex];
}

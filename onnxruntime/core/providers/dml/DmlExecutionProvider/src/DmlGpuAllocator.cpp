// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include "precomp.h"
#include "DmlGpuAllocator.h"
#include "core/framework/allocator.h"
#include "DmlReservedResourceSubAllocator.h"
#include "DmlTaggedPointer.h"
#include "DmlAllocationInfo.h"
#include "BucketizedBufferAllocator.h"

namespace Dml
{
    DmlGpuAllocator::DmlGpuAllocator(
        onnxruntime::IAllocator* bfcAllocator,
        BucketizedBufferAllocator* bucketizedBufferAllocator,
        std::shared_ptr<DmlReservedResourceSubAllocator> bfcSubAllocator,
        ActiveAllocator activeAllocator)
    : onnxruntime::IAllocator(
        OrtMemoryInfo(
            onnxruntime::DML,
            OrtAllocatorType::OrtDeviceAllocator,
            OrtDevice(OrtDevice::GPU, OrtDevice::MemType::DEFAULT, 0),
            0
        )
    ),
    m_bfcAllocator(bfcAllocator),
    m_bucketizedBufferAllocator(bucketizedBufferAllocator),
    m_bfcSubAllocator(bfcSubAllocator),
    m_activeAllocator(activeAllocator) {}

    void* DmlGpuAllocator::Alloc(size_t sizeInBytes)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            return m_bfcAllocator->Alloc(sizeInBytes);
        case ActiveAllocator::BucketizedBufferAllocator:
            return m_bucketizedBufferAllocator->Alloc(sizeInBytes);
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

    void DmlGpuAllocator::Free(void* ptr)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            return m_bfcAllocator->Free(ptr);
        case ActiveAllocator::BucketizedBufferAllocator:
            return m_bucketizedBufferAllocator->Free(ptr);
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

    D3D12BufferRegion DmlGpuAllocator::CreateBufferRegion(void* opaquePointer, uint64_t sizeInBytes)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            return m_bfcSubAllocator->CreateBufferRegion(opaquePointer, sizeInBytes);
        case ActiveAllocator::BucketizedBufferAllocator:
            return m_bucketizedBufferAllocator->CreateBufferRegion(opaquePointer, sizeInBytes);
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

    AllocationInfo* DmlGpuAllocator::GetAllocationInfo(void* opaquePointer)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            return m_bfcSubAllocator->GetAllocationInfo(opaquePointer);
        case ActiveAllocator::BucketizedBufferAllocator:
            return m_bucketizedBufferAllocator->GetAllocationInfo(opaquePointer);
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

    void DmlGpuAllocator::SetDefaultRoundingMode(AllocatorRoundingMode roundingMode)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            m_bfcSubAllocator->SetDefaultRoundingMode(roundingMode);
            break;
        case ActiveAllocator::BucketizedBufferAllocator:
            m_bucketizedBufferAllocator->SetDefaultRoundingMode(roundingMode);
            break;
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

    DmlBuffer DmlGpuAllocator::AllocateDefaultBuffer(uint64_t num_bytes)
    {
        return DmlBuffer(this, num_bytes);
    }

    uint64_t DmlGpuAllocator::GetUniqueId(void* opaquePointer)
    {
        switch(m_activeAllocator)
        {
        case ActiveAllocator::BfcAllocator:
            return m_bfcSubAllocator->GetUniqueId(opaquePointer);
        case ActiveAllocator::BucketizedBufferAllocator:
            return m_bucketizedBufferAllocator->GetUniqueId(opaquePointer);
        default:
            ORT_THROW_HR(E_UNEXPECTED);
        }
    }

} // namespace Dml
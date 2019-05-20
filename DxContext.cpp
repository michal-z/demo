#pragma once

struct DxContext
{
    prv struct DescriptorHeap
    {
        ID3D12DescriptorHeap* heap;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_start;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_start;
        u32 size;
        u32 capacity;
    };

    pub ID3D12Device2* device;
    pub ID3D12GraphicsCommandList* cmdlist;
    pub ID3D12CommandQueue* cmdqueue;
    pub ID3D12CommandAllocator* cmdalloc[2];
    pub u32 resolution[2];
    pub u32 descriptor_size;
    pub u32 descriptor_size_rtv;
    pub u32 frame_index;
    prv u32 back_buffer_index;
    prv ID3D12Resource* swapbuffers[4];
    prv IDXGISwapChain3* swapchain;
    prv ID3D12Resource* ds_buffer;
    prv DescriptorHeap rt_heap;
    prv DescriptorHeap ds_heap;
    prv DescriptorHeap cpu_descriptor_heap;
    prv DescriptorHeap gpu_descriptor_heaps[2];
    prv ID3D12Fence* frame_fence;
    prv HANDLE frame_fence_event;
    prv u64 frame_count;

    pub fn void init(DxContext& self, HWND window)
    {
        IDXGIFactory4* factory;
#ifdef _DEBUG
        VHR(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory)));
#else
        VHR(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));
#endif
#ifdef _DEBUG
        {
            ID3D12Debug* dbg;
            D3D12GetDebugInterface(IID_PPV_ARGS(&dbg));
            if (dbg) {
                dbg->EnableDebugLayer();
                ID3D12Debug1* dbg1;
                dbg->QueryInterface(IID_PPV_ARGS(&dbg1));
                if (dbg1) {
                    dbg1->SetEnableGPUBasedValidation(TRUE);
                }
                SAFE_RELEASE(dbg);
                SAFE_RELEASE(dbg1);
            }
        }
#endif
        if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&self.device)))) {
            MessageBox(window, "This application requires Windows 10 1709 (RS3) or newer.", "D3D12CreateDevice failed", MB_OK | MB_ICONERROR);
            return;
        }

        D3D12_COMMAND_QUEUE_DESC cmdqueue_desc = {};
        cmdqueue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        cmdqueue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        cmdqueue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        VHR(self.device->CreateCommandQueue(&cmdqueue_desc, IID_PPV_ARGS(&self.cmdqueue)));

        DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
        swapchain_desc.BufferCount = 4;
        swapchain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapchain_desc.OutputWindow = window;
        swapchain_desc.SampleDesc.Count = 1;
        swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapchain_desc.Windowed = TRUE;

        IDXGISwapChain* temp_swapchain;
        VHR(factory->CreateSwapChain(self.cmdqueue, &swapchain_desc, &temp_swapchain));
        VHR(temp_swapchain->QueryInterface(IID_PPV_ARGS(&self.swapchain)));
        SAFE_RELEASE(temp_swapchain);
        SAFE_RELEASE(factory);

        RECT rect;
        GetClientRect(window, &rect);
        self.resolution[0] = (u32)rect.right;
        self.resolution[1] = (u32)rect.bottom;

        for (u32 i = 0; i < 2; ++i) {
            VHR(self.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&self.cmdalloc[i])));
        }

        self.descriptor_size = self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        self.descriptor_size_rtv = self.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        init_descriptor_heaps(self);

        /* swap buffer render targets */ {
            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            allocate_descriptors(self, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 4, handle);

            for (u32 i = 0; i < 4; ++i) {
                VHR(self.swapchain->GetBuffer(i, IID_PPV_ARGS(&self.swapbuffers[i])));
                self.device->CreateRenderTargetView(self.swapbuffers[i], nullptr, handle);
                handle.ptr += self.descriptor_size_rtv;
            }
        }
        /* depth-stencil target */ {
            auto image_desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, self.resolution[0], self.resolution[1]);
            image_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
            VHR(self.device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &image_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0), IID_PPV_ARGS(&self.ds_buffer)));

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            allocate_descriptors(self, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, handle);

            D3D12_DEPTH_STENCIL_VIEW_DESC view_desc = {};
            view_desc.Format = DXGI_FORMAT_D32_FLOAT;
            view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            view_desc.Flags = D3D12_DSV_FLAG_NONE;
            self.device->CreateDepthStencilView(self.ds_buffer, &view_desc, handle);
        }

        VHR(self.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, self.cmdalloc[0], nullptr, IID_PPV_ARGS(&self.cmdlist)));
        self.cmdlist->Close();

        VHR(self.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&self.frame_fence)));
        self.frame_fence_event = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    }

    pub fn void shutdown(DxContext& self)
    {
        // @Incomplete: Release all resources.
        SAFE_RELEASE(self.cmdlist);
        SAFE_RELEASE(self.cmdalloc[0]);
        SAFE_RELEASE(self.cmdalloc[1]);
        SAFE_RELEASE(self.rt_heap.heap);
        SAFE_RELEASE(self.ds_heap.heap);
        for (u32 i = 0; i < 4; ++i) {
            SAFE_RELEASE(self.swapbuffers[i]);
        }
        CloseHandle(self.frame_fence_event);
        SAFE_RELEASE(self.frame_fence);
        SAFE_RELEASE(self.swapchain);
        SAFE_RELEASE(self.cmdqueue);
        SAFE_RELEASE(self.device);
    }

    pub fn inline void get_back_buffer(const DxContext& self, ID3D12Resource*& buffer_out, D3D12_CPU_DESCRIPTOR_HANDLE& descriptor_out)
    {
        buffer_out = self.swapbuffers[self.back_buffer_index];
        descriptor_out = self.rt_heap.cpu_start;
        descriptor_out.ptr += self.back_buffer_index * self.descriptor_size_rtv;
    }

    pub fn inline void get_depthstencil_buffer(const DxContext& self, ID3D12Resource*& buffer_out, D3D12_CPU_DESCRIPTOR_HANDLE& descriptor_out)
    {
        buffer_out = self.ds_buffer;
        descriptor_out = self.ds_heap.cpu_start;
    }

    pub fn void allocate_descriptors(DxContext& self, D3D12_DESCRIPTOR_HEAP_TYPE type, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& cpu_out)
    {
        u32 descriptor_size;
        DescriptorHeap& heap = get_descriptor_heap(self, type, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, descriptor_size);
        assert((heap.size + count) < heap.capacity);
        cpu_out.ptr = heap.cpu_start.ptr + heap.size * descriptor_size;
        heap.size += count;
    }

    pub fn void allocate_descriptors(DxContext& self, u32 count, D3D12_CPU_DESCRIPTOR_HANDLE& cpu_out, D3D12_GPU_DESCRIPTOR_HANDLE& gpu_out)
    {
        u32 descriptor_size;
        DescriptorHeap& heap = get_descriptor_heap(self, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, descriptor_size);
        assert((heap.size + count) < heap.capacity);

        cpu_out.ptr = heap.cpu_start.ptr + heap.size * descriptor_size;
        gpu_out.ptr = heap.gpu_start.ptr + heap.size * descriptor_size;

        heap.size += count;
    }

    pub fn void present(DxContext& self)
    {
        self.swapchain->Present(0, 0);
        self.cmdqueue->Signal(self.frame_fence, ++self.frame_count);

        const u64 gpu_frame_count = self.frame_fence->GetCompletedValue();

        if ((self.frame_count - gpu_frame_count) >= 2) {
            self.frame_fence->SetEventOnCompletion(gpu_frame_count + 1, self.frame_fence_event);
            WaitForSingleObject(self.frame_fence_event, INFINITE);
        }

        self.frame_index = !self.frame_index;
        self.back_buffer_index = self.swapchain->GetCurrentBackBufferIndex();
        self.gpu_descriptor_heaps[self.frame_index].size = 0;
    }

    pub fn void wait_for_frame_fence(DxContext& self)
    {
        self.cmdqueue->Signal(self.frame_fence, ++self.frame_count);
        self.frame_fence->SetEventOnCompletion(self.frame_count, self.frame_fence_event);
        WaitForSingleObject(self.frame_fence_event, INFINITE);
    }

    pub fn inline void set_descriptor_heap(const DxContext& self)
    {
        self.cmdlist->SetDescriptorHeaps(1, &self.gpu_descriptor_heaps[self.frame_index].heap);
    }

    prv fn DescriptorHeap& get_descriptor_heap(DxContext& self, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, u32& descriptor_size_out)
    {
        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV) {
            descriptor_size_out = self.descriptor_size_rtv;
            return self.rt_heap;
        } else if (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV) {
            descriptor_size_out = self.descriptor_size_rtv;
            return self.ds_heap;
        } else if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            descriptor_size_out = self.descriptor_size;
            if (flags == D3D12_DESCRIPTOR_HEAP_FLAG_NONE) {
                return self.cpu_descriptor_heap;
            } else if (flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
                return self.gpu_descriptor_heaps[self.frame_index];
            }
        }
        assert(0);
        descriptor_size_out = 0;
        return self.cpu_descriptor_heap;
    }

    prv fn void init_descriptor_heaps(DxContext& self)
    {
        /* render target descriptor heap (RTV) */ {
            self.rt_heap.capacity = 16;

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.NumDescriptors = self.rt_heap.capacity;
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            VHR(self.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&self.rt_heap.heap)));
            self.rt_heap.cpu_start = self.rt_heap.heap->GetCPUDescriptorHandleForHeapStart();
        }
        /* depth-stencil descriptor heap (DSV) */ {
            self.ds_heap.capacity = 8;

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.NumDescriptors = self.ds_heap.capacity;
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            VHR(self.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&self.ds_heap.heap)));
            self.ds_heap.cpu_start = self.ds_heap.heap->GetCPUDescriptorHandleForHeapStart();
        }
        /* non-shader visible descriptor heap (CBV, SRV, UAV) */ {
            self.cpu_descriptor_heap.capacity = 10000;

            D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
            heap_desc.NumDescriptors = self.cpu_descriptor_heap.capacity;
            heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            VHR(self.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&self.cpu_descriptor_heap.heap)));
            self.cpu_descriptor_heap.cpu_start = self.cpu_descriptor_heap.heap->GetCPUDescriptorHandleForHeapStart();
        }
        /* shader visible descriptor heaps (CBV, SRV, UAV) */ {
            for (u32 i = 0; i < 2; ++i) {
                self.gpu_descriptor_heaps[i].capacity = 10000;

                D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
                heap_desc.NumDescriptors = self.gpu_descriptor_heaps[i].capacity;
                heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
                heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
                VHR(self.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&self.gpu_descriptor_heaps[i].heap)));

                self.gpu_descriptor_heaps[i].cpu_start = self.gpu_descriptor_heaps[i].heap->GetCPUDescriptorHandleForHeapStart();
                self.gpu_descriptor_heaps[i].gpu_start = self.gpu_descriptor_heaps[i].heap->GetGPUDescriptorHandleForHeapStart();
            }
        }
    }
};

struct Demo;
#pragma once
#include "DxContext.cpp"
#include "Common.cpp"

struct Demo {
    pub static constexpr char* k_name = "Rasterization";
    prv static constexpr u32 k_num_pipelines = 1;

    prv ID3D12Resource* fragments_buffer;
    prv D3D12_CPU_DESCRIPTOR_HANDLE fragments_uav;
    prv D3D12_CPU_DESCRIPTOR_HANDLE fragments_srv;
    prv ID3D12PipelineState* pipelines[k_num_pipelines];
    prv ID3D12RootSignature* rootsignatures[k_num_pipelines];

    pub fn i32 run(Demo& self) {
        HWND window = Common::createWindow(k_name, 1920, 1080);

        DxContext dx = {};
        DxContext::init(dx, window);

        init(self, dx);

        for (;;) {
            MSG message = {};
            if (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
                DispatchMessage(&message);
                if (message.message == WM_QUIT) {
                    break;
                }
            } else {
                f64 time;
                f32 delta_time;
                Common::updateFrameStats(window, k_name, time, delta_time);
                draw(self, dx);
                DxContext::present(dx);
            }
        }
        return 0;
    }

    prv fn void init(Demo& self, DxContext& dx) {
        /* VS_0, PS_0 */ {
            std::vector<u8> vs_code = Common::loadFile("Data/Shaders/0.vs.cso");
            std::vector<u8> ps_code = Common::loadFile("Data/Shaders/0.ps.cso");

            D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.VS = { vs_code.data(), vs_code.size() };
            pso_desc.PS = { ps_code.data(), ps_code.size() };
            pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
            pso_desc.SampleMask = 0xffffffff;
            pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            pso_desc.NumRenderTargets = 1;
            pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            pso_desc.SampleDesc.Count = 1;

            VHR(dx.device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&self.pipelines[0])));
            VHR(dx.device->CreateRootSignature(0, vs_code.data(), vs_code.size(), IID_PPV_ARGS(&self.rootsignatures[0])));
        }
        /* buffer that stores window-space position of each fragment */ {
            auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer((dx.resolution[0] * dx.resolution[1] + 1) * sizeof(XMFLOAT2));
            buffer_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            VHR(dx.device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS, &buffer_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&self.fragments_buffer)));

            DxContext::allocateDescriptors(dx, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, self.fragments_srv);
            DxContext::allocateDescriptors(dx, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, self.fragments_uav);

            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc = {};
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.FirstElement = 1;
            uav_desc.Buffer.NumElements = dx.resolution[0] * dx.resolution[1];
            uav_desc.Buffer.StructureByteStride = sizeof(XMFLOAT2);
            uav_desc.Buffer.CounterOffsetInBytes = 0;
            dx.device->CreateUnorderedAccessView(self.fragments_buffer, self.fragments_buffer, &uav_desc, self.fragments_uav);

            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Buffer.FirstElement = 1;
            srv_desc.Buffer.NumElements = dx.resolution[0] * dx.resolution[1];
            srv_desc.Buffer.StructureByteStride = sizeof(XMFLOAT2);
            dx.device->CreateShaderResourceView(self.fragments_buffer, &srv_desc, self.fragments_srv);
        }
    }

    prv fn void draw(Demo& self, DxContext& dx) {
        ID3D12GraphicsCommandList2* cmdlist = dx.cmdlist;

        dx.cmdalloc[dx.frame_index]->Reset();
        cmdlist->Reset(dx.cmdalloc[dx.frame_index], nullptr);

        DxContext::setDescriptorHeap(dx);

        cmdlist->RSSetViewports(1, &D3D12_VIEWPORT{ 0.0f, 0.0f, (f32)dx.resolution[0], (f32)dx.resolution[1], 0.0f, 1.0f });
        cmdlist->RSSetScissorRects(1, &D3D12_RECT{ 0, 0, (LONG)dx.resolution[0], (LONG)dx.resolution[1] });

        ID3D12Resource* back_buffer;
        D3D12_CPU_DESCRIPTOR_HANDLE back_buffer_descriptor;
        DxContext::getBackBuffer(dx, back_buffer, back_buffer_descriptor);

        cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        /* clear atomic counter in the fragments buffer */ {
            cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(self.fragments_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST));

            D3D12_WRITEBUFFERIMMEDIATE_PARAMETER param = { self.fragments_buffer->GetGPUVirtualAddress(), 0 };
            cmdlist->WriteBufferImmediate(1, &param, nullptr);

            cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(self.fragments_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
        }

        cmdlist->OMSetRenderTargets(1, &back_buffer_descriptor, 0, nullptr);
        cmdlist->ClearRenderTargetView(back_buffer_descriptor, XMVECTORF32{ 0.0f, 0.2f, 0.4f, 1.0f }, 0, nullptr);

        cmdlist->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdlist->SetPipelineState(self.pipelines[0]);
        cmdlist->SetGraphicsRootSignature(self.rootsignatures[0]);
        cmdlist->DrawInstanced(3, 1, 0, 0);

        cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        cmdlist->Close();

        dx.cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmdlist);
    }
};

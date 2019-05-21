#pragma once
#include "Common.cpp"
#include "DxContext.cpp"

struct Demo {
    pub static constexpr char* k_name = "Demo";
    prv static constexpr u32 k_num_pipelines = 1;

    prv ID3D12Resource* upload_buffers[2];
    prv ID3D12PipelineState* pipelines[k_num_pipelines];
    prv ID3D12RootSignature* rootsignatures[k_num_pipelines];

    pub fn i32 run(Demo& self) {
        HWND window = Common::create_window(k_name, 1920, 1080);

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
                Common::update_frame_stats(window, k_name, time, delta_time);
                draw(self, dx);
                DxContext::present(dx);
            }
        }
        return 0;
    }

    prv fn void init(Demo& self, DxContext& dx) {
        /* VS_0, PS_0 */ {
            std::vector<u8> vs_code = Common::load_file("Data/Shaders/0.vs.cso");
            std::vector<u8> ps_code = Common::load_file("Data/Shaders/0.ps.cso");

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

        for (u32 i = 0; i < 2; ++i) {
            VHR(dx.device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(64 * 1024), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&self.upload_buffers[i])));
        }
    }

    prv fn void draw(Demo& self, DxContext& dx) {
        ID3D12GraphicsCommandList* cmdlist = dx.cmdlist;

        dx.cmdalloc[dx.frame_index]->Reset();
        cmdlist->Reset(dx.cmdalloc[dx.frame_index], nullptr);

        DxContext::set_descriptor_heap(dx);

        cmdlist->RSSetViewports(1, &D3D12_VIEWPORT{ 0.0f, 0.0f, (f32)dx.resolution[0], (f32)dx.resolution[1], 0.0f, 1.0f });
        cmdlist->RSSetScissorRects(1, &D3D12_RECT{ 0, 0, (LONG)dx.resolution[0], (LONG)dx.resolution[1] });

        ID3D12Resource* back_buffer;
        D3D12_CPU_DESCRIPTOR_HANDLE back_buffer_descriptor;
        DxContext::get_back_buffer(dx, back_buffer, back_buffer_descriptor);

        cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        cmdlist->OMSetRenderTargets(1, &back_buffer_descriptor, 0, nullptr);
        cmdlist->ClearRenderTargetView(back_buffer_descriptor, XMVECTORF32{ 0.0f, 0.2f, 0.4f, 1.0f }, 0, nullptr);

        cmdlist->SetPipelineState(self.pipelines[0]);
        cmdlist->SetGraphicsRootSignature(self.rootsignatures[0]);
        cmdlist->DrawInstanced(3, 1, 0, 0);

        cmdlist->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(back_buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        cmdlist->Close();

        dx.cmdqueue->ExecuteCommandLists(1, (ID3D12CommandList**)&cmdlist);
    }
};

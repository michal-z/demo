#pragma once

struct Common {
    pub fn std::vector<u8> loadFile(const char* filename) {
        FILE* file = fopen(filename, "rb");
        assert(file);
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        assert(size != -1);
        std::vector<u8> content(size);
        fseek(file, 0, SEEK_SET);
        fread(&content[0], 1, content.size(), file);
        fclose(file);
        return content;
    }

    pub fn void updateFrameStats(HWND window, const char* name, f64& time_out, f32& delta_time_out) {
        static f64 previous_time = -1.0;
        static f64 header_refresh_time = 0.0;
        static u32 frame_count = 0;

        if (previous_time < 0.0) {
            previous_time = getTime();
            header_refresh_time = previous_time;
        }

        time_out = getTime();
        delta_time_out = (f32)(time_out - previous_time);
        previous_time = time_out;

        if ((time_out - header_refresh_time) >= 1.0) {
            const f64 fps = frame_count / (time_out - header_refresh_time);
            const f64 ms = (1.0 / fps) * 1000.0;
            char header[256];
            snprintf(header, sizeof(header), "[%.1f fps  %.3f ms] %s", fps, ms, name);
            SetWindowText(window, header);
            header_refresh_time = time_out;
            frame_count = 0;
        }
        frame_count++;
    }

    pub fn f64 getTime() {
        static LARGE_INTEGER start_counter;
        static LARGE_INTEGER frequency;
        if (start_counter.QuadPart == 0) {
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&start_counter);
        }
        LARGE_INTEGER counter;
        QueryPerformanceCounter(&counter);
        return (counter.QuadPart - start_counter.QuadPart) / (f64)frequency.QuadPart;
    }

    pub fn HWND createWindow(const char* name, u32 width, u32 height) {
        WNDCLASS winclass = {};
        winclass.lpfnWndProc = processWindowMessage;
        winclass.hInstance = GetModuleHandle(nullptr);
        winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
        winclass.lpszClassName = name;
        if (!RegisterClass(&winclass)) {
            assert(0);
        }

        RECT rect = { 0, 0, (LONG)width, (LONG)height };
        if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0)) {
            assert(0);
        }

        HWND window = CreateWindowEx(0, name, name, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, nullptr, 0);
        assert(window);

        return window;
    }

    prv fn LRESULT CALLBACK processWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
        }
        return DefWindowProc(window, message, wparam, lparam);
    }
};

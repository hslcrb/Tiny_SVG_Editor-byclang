/*
 * Tiny SVG Editor (Win32 API)
 * * [컴파일 방법]
 * 1. MinGW (GCC) 사용 시:
 * gcc -O2 -o svg_editor.exe svg_editor.c -mwindows -lgdi32 -luser32 -lcomdlg32
 * * 2. Visual Studio (MSVC) 개발자 명령 프롬프트 사용 시:
 * cl /O2 svg_editor.c user32.lib gdi32.lib comdlg32.lib
 */

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>

// --- 데이터 구조 및 열거형 ---
typedef enum {
    TOOL_LINE,
    TOOL_RECT,
    TOOL_ELLIPSE
} ToolType;

typedef struct {
    ToolType type;
    int x1, y1;
    int x2, y2;
    COLORREF color;
    int thickness;
} Shape;

#define MAX_SHAPES 5000
#define TOOLBAR_HEIGHT 50

// --- 전역 변수 ---
Shape g_shapes[MAX_SHAPES];
int g_shapeCount = 0;

ToolType g_currentTool = TOOL_LINE;
COLORREF g_currentColor = RGB(0, 0, 0); // 기본색: 검정
int g_currentThickness = 2;

BOOL g_isDrawing = FALSE;
int g_startX, g_startY;
int g_currentX, g_currentY;

// UI 컨트롤 ID
#define ID_BTN_LINE    101
#define ID_BTN_RECT    102
#define ID_BTN_ELLIPSE 103
#define ID_BTN_BLACK   104
#define ID_BTN_RED     105
#define ID_BTN_BLUE    106
#define ID_BTN_GREEN   107
#define ID_BTN_CLEAR   108
#define ID_BTN_SAVE    109

// --- SVG 저장 함수 ---
void SaveToSVG(HWND hwnd, const char* filename) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    int width = rc.right;
    int height = rc.bottom - TOOLBAR_HEIGHT;

    FILE* f = fopen(filename, "w");
    if (!f) {
        MessageBoxA(hwnd, "파일을 열 수 없습니다.", "오류", MB_OK | MB_ICONERROR);
        return;
    }

    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\">\n", width, height);
    fprintf(f, "  <rect width=\"100%%\" height=\"100%%\" fill=\"white\" />\n"); // 배경

    for (int i = 0; i < g_shapeCount; i++) {
        Shape s = g_shapes[i];
        int r = GetRValue(s.color);
        int g = GetGValue(s.color);
        int b = GetBValue(s.color);
        
        // 툴바 영역만큼 y좌표 보정
        s.y1 -= TOOLBAR_HEIGHT;
        s.y2 -= TOOLBAR_HEIGHT;

        if (s.type == TOOL_LINE) {
            fprintf(f, "  <line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%d\" stroke-linecap=\"round\" />\n",
                    s.x1, s.y1, s.x2, s.y2, r, g, b, s.thickness);
        } else if (s.type == TOOL_RECT) {
            int x = min(s.x1, s.x2);
            int y = min(s.y1, s.y2);
            int w = abs(s.x2 - s.x1);
            int h = abs(s.y2 - s.y1);
            fprintf(f, "  <rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"none\" stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%d\" />\n",
                    x, y, w, h, r, g, b, s.thickness);
        } else if (s.type == TOOL_ELLIPSE) {
            int cx = (s.x1 + s.x2) / 2;
            int cy = (s.y1 + s.y2) / 2;
            int rx = abs(s.x2 - s.x1) / 2;
            int ry = abs(s.y2 - s.y1) / 2;
            fprintf(f, "  <ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" fill=\"none\" stroke=\"rgb(%d,%d,%d)\" stroke-width=\"%d\" />\n",
                    cx, cy, rx, ry, r, g, b, s.thickness);
        }
    }

    fprintf(f, "</svg>\n");
    fclose(f);
    MessageBoxA(hwnd, "SVG 파일로 성공적으로 저장되었습니다.", "저장 완료", MB_OK | MB_ICONINFORMATION);
}

// --- 도구 렌더링 함수 ---
void DrawShape(HDC hdc, Shape s) {
    HPEN hPen = CreatePen(PS_SOLID, s.thickness, s.color);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH)); // 채우기 없음

    if (s.type == TOOL_LINE) {
        MoveToEx(hdc, s.x1, s.y1, NULL);
        LineTo(hdc, s.x2, s.y2);
    } else if (s.type == TOOL_RECT) {
        Rectangle(hdc, s.x1, s.y1, s.x2, s.y2);
    } else if (s.type == TOOL_ELLIPSE) {
        Ellipse(hdc, s.x1, s.y1, s.x2, s.y2);
    }

    SelectObject(hdc, hOldBrush);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

// --- 메인 윈도우 프로시저 ---
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // UI 버튼 생성
            int x = 10;
            CreateWindowA("BUTTON", "선", WS_VISIBLE | WS_CHILD, x, 10, 60, 30, hwnd, (HMENU)ID_BTN_LINE, NULL, NULL); x += 65;
            CreateWindowA("BUTTON", "사각형", WS_VISIBLE | WS_CHILD, x, 10, 60, 30, hwnd, (HMENU)ID_BTN_RECT, NULL, NULL); x += 65;
            CreateWindowA("BUTTON", "원", WS_VISIBLE | WS_CHILD, x, 10, 60, 30, hwnd, (HMENU)ID_BTN_ELLIPSE, NULL, NULL); x += 80;
            
            CreateWindowA("BUTTON", "검정", WS_VISIBLE | WS_CHILD, x, 10, 50, 30, hwnd, (HMENU)ID_BTN_BLACK, NULL, NULL); x += 55;
            CreateWindowA("BUTTON", "빨강", WS_VISIBLE | WS_CHILD, x, 10, 50, 30, hwnd, (HMENU)ID_BTN_RED, NULL, NULL); x += 55;
            CreateWindowA("BUTTON", "파랑", WS_VISIBLE | WS_CHILD, x, 10, 50, 30, hwnd, (HMENU)ID_BTN_BLUE, NULL, NULL); x += 55;
            CreateWindowA("BUTTON", "초록", WS_VISIBLE | WS_CHILD, x, 10, 50, 30, hwnd, (HMENU)ID_BTN_GREEN, NULL, NULL); x += 80;

            CreateWindowA("BUTTON", "지우기", WS_VISIBLE | WS_CHILD, x, 10, 60, 30, hwnd, (HMENU)ID_BTN_CLEAR, NULL, NULL); x += 70;
            CreateWindowA("BUTTON", "SVG 저장", WS_VISIBLE | WS_CHILD, x, 10, 80, 30, hwnd, (HMENU)ID_BTN_SAVE, NULL, NULL);
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case ID_BTN_LINE:    g_currentTool = TOOL_LINE; break;
                case ID_BTN_RECT:    g_currentTool = TOOL_RECT; break;
                case ID_BTN_ELLIPSE: g_currentTool = TOOL_ELLIPSE; break;
                case ID_BTN_BLACK:   g_currentColor = RGB(0, 0, 0); break;
                case ID_BTN_RED:     g_currentColor = RGB(255, 0, 0); break;
                case ID_BTN_BLUE:    g_currentColor = RGB(0, 0, 255); break;
                case ID_BTN_GREEN:   g_currentColor = RGB(0, 128, 0); break;
                case ID_BTN_CLEAR: 
                    g_shapeCount = 0; 
                    InvalidateRect(hwnd, NULL, TRUE); 
                    break;
                case ID_BTN_SAVE: {
                    char szFileName[MAX_PATH] = "drawing.svg";
                    OPENFILENAMEA ofn;
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = hwnd;
                    ofn.lpstrFilter = "SVG Files (*.svg)\0*.svg\0All Files (*.*)\0*.*\0";
                    ofn.lpstrFile = szFileName;
                    ofn.nMaxFile = MAX_PATH;
                    ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
                    ofn.lpstrDefExt = "svg";
                    
                    if (GetSaveFileNameA(&ofn)) {
                        SaveToSVG(hwnd, szFileName);
                    }
                    break;
                }
            }
            break;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);
            // 툴바 아래 영역에서만 그리기 허용
            if (y > TOOLBAR_HEIGHT && g_shapeCount < MAX_SHAPES) {
                g_isDrawing = TRUE;
                g_startX = x; g_startY = y;
                g_currentX = x; g_currentY = y;
                SetCapture(hwnd); // 마우스가 창 밖으로 나가도 이벤트를 받도록 캡처
            }
            break;
        }

        case WM_MOUSEMOVE: {
            if (g_isDrawing) {
                g_currentX = GET_X_LPARAM(lParam);
                g_currentY = GET_Y_LPARAM(lParam);
                // 실시간 미리보기를 위해 화면 갱신
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case WM_LBUTTONUP: {
            if (g_isDrawing) {
                g_isDrawing = FALSE;
                ReleaseCapture();

                // 마우스 클릭 해제 시 도형 확정 저장
                Shape s;
                s.type = g_currentTool;
                s.x1 = g_startX; s.y1 = g_startY;
                s.x2 = g_currentX; s.y2 = g_currentY;
                s.color = g_currentColor;
                s.thickness = g_currentThickness;
                
                // 크기가 0인 객체 방지
                if (s.x1 != s.x2 || s.y1 != s.y2) {
                    g_shapes[g_shapeCount++] = s;
                }
                InvalidateRect(hwnd, NULL, FALSE);
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 더블 버퍼링 (화면 깜빡임 방지)
            RECT rc;
            GetClientRect(hwnd, &rc);
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hbmMem);

            // 1. 전체 화면을 흰색으로 지우기
            HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(hdcMem, &rc, hBgBrush);
            DeleteObject(hBgBrush);

            // 2. 상단 툴바 배경색 칠하기 (회색)
            RECT tbRect = {0, 0, rc.right, TOOLBAR_HEIGHT};
            HBRUSH hTbBrush = CreateSolidBrush(RGB(240, 240, 240));
            FillRect(hdcMem, &tbRect, hTbBrush);
            DeleteObject(hTbBrush);

            // 3. 저장된 도형들 그리기
            for (int i = 0; i < g_shapeCount; i++) {
                DrawShape(hdcMem, g_shapes[i]);
            }

            // 4. 현재 드래그 중인 도형 (미리보기) 그리기
            if (g_isDrawing) {
                Shape tempShape;
                tempShape.type = g_currentTool;
                tempShape.x1 = g_startX; tempShape.y1 = g_startY;
                tempShape.x2 = g_currentX; tempShape.y2 = g_currentY;
                tempShape.color = g_currentColor;
                tempShape.thickness = g_currentThickness;
                DrawShape(hdcMem, tempShape);
            }

            // 5. 메모리 DC에서 화면 DC로 복사 (BitBlt)
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

            // 메모리 정리
            SelectObject(hdcMem, hOldBitmap);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);
            EndPaint(hwnd, &ps);
            break;
        }

        // 창 크기가 변경될 때 화면 전체 다시 그리기 요청
        case WM_SIZE:
            InvalidateRect(hwnd, NULL, FALSE);
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// --- 메인 진입점 ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "TinySVGEditorClass";

    WNDCLASSA wc = {0};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor       = LoadCursor(NULL, IDC_CROSS); // 십자 커서 사용
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

    RegisterClassA(&wc);

    HWND hwnd = CreateWindowExA(
        0,                              // 확장 스타일
        CLASS_NAME,                     // 윈도우 클래스 이름
        "Tiny SVG Editor (순수 C / Win32 API)", // 창 제목
        WS_OVERLAPPEDWINDOW,            // 창 스타일
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, // x, y, width, height
        NULL,                           // 부모 창
        NULL,                           // 메뉴
        hInstance,                      // 인스턴스 핸들
        NULL                            // 추가 파라미터
    );

    if (hwnd == NULL) {
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    // 메시지 루프
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
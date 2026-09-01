#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cmath>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int kWidth = 1500;
constexpr int kHeight = 850;
constexpr int kButtonWidth = 150;
constexpr int kButtonHeight = 25;
constexpr int kButtonMargin = 10;
constexpr int kBarAreaRightPadding = 200;
constexpr int kBarAreaBottomPadding = kButtonHeight + 2 * kButtonMargin;

constexpr COLORREF kWhite = RGB(255, 255, 255);
constexpr COLORREF kBlue = RGB(0, 0, 255);
constexpr COLORREF kGray = RGB(200, 200, 200);
constexpr COLORREF kDarkGray = RGB(50, 50, 50);
constexpr COLORREF kGreen = RGB(0, 255, 0);
constexpr COLORREF kRed = RGB(255, 0, 0);

enum ControlId {
    ID_SIZE = 100,
    ID_SPEED,
    ID_SIZE_SCROLL,
    ID_SPEED_SCROLL,
    ID_STOP,
    ID_SHUFFLE,
    ID_BUBBLE,
    ID_SELECTION,
    ID_QUICK,
    ID_HEAP,
    ID_PANCAKE,
    ID_MERGE,
    ID_COCKTAIL,
    ID_RADIX,
    ID_INSERTION,
    ID_TIM,
    ID_RADIX_MERGE,
    ID_RADIX_HEAP,
    ID_SILLY,
};

HWND gMainWindow = nullptr;
HWND gSizeEdit = nullptr;
HWND gSpeedEdit = nullptr;
HWND gSizeScroll = nullptr;
HWND gSpeedScroll = nullptr;
HWND gStopButton = nullptr;

std::vector<int> gArray;
std::mutex gArrayMutex;
std::thread gWorker;
std::atomic<bool> gSorting{false};
std::atomic<bool> gAbort{false};
std::atomic<int> gSpeed{90};
std::atomic<bool> gAudioRunning{false};
int gSize = 100;
int gHighlight1 = -1;
int gHighlight2 = -1;
thread_local std::vector<int>* gActiveArray = nullptr;
thread_local int gStepCounter = 0;
thread_local std::chrono::steady_clock::time_point gLastFrameTime{};
thread_local std::chrono::steady_clock::time_point gLastToneTime{};

std::mutex gAudioMutex;
std::condition_variable gAudioCv;
std::thread gAudioThread;
int gPendingFrequency = 0;
bool gHasPendingTone = false;

HDC gBackBufferDc = nullptr;
HBITMAP gBackBufferBitmap = nullptr;
HGDIOBJ gBackBufferOldBitmap = nullptr;
int gBackBufferWidth = 0;
int gBackBufferHeight = 0;

int barWidth() {
    return std::max(1, (kWidth - kBarAreaRightPadding) / std::max(1, gSize));
}

int maxBarHeight() {
    return kHeight - kBarAreaBottomPadding;
}

int clampInt(int value, int low, int high) {
    return std::max(low, std::min(value, high));
}

int toneForIndex(int index, int size) {
    size = std::max(1, size);
    return 200 + static_cast<int>(index * (880 - 200) / static_cast<double>(size));
}

int toneForValue(int value) {
    return 200 + static_cast<int>(value * (880 - 200) / static_cast<double>(std::max(1, maxBarHeight())));
}

std::vector<int16_t> makeToneBuffer(int frequency, int durationMs) {
    constexpr int sampleRate = 44100;
    int samples = std::max(1, sampleRate * durationMs / 1000);
    int fadeSamples = std::min(samples / 2, sampleRate / 250);
    std::vector<int16_t> buffer(samples);
    double phaseStep = 2.0 * 3.14159265358979323846 * frequency / sampleRate;

    for (int i = 0; i < samples; ++i) {
        double envelope = 1.0;
        if (fadeSamples > 0 && i < fadeSamples) {
            envelope = i / static_cast<double>(fadeSamples);
        } else if (fadeSamples > 0 && i >= samples - fadeSamples) {
            envelope = (samples - i - 1) / static_cast<double>(fadeSamples);
        }
        double sample = std::sin(i * phaseStep) * envelope * 0.18;
        buffer[i] = static_cast<int16_t>(sample * 32767);
    }
    return buffer;
}

void audioLoop() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = 44100;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HWAVEOUT device = nullptr;
    if (waveOutOpen(&device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return;
    }

    while (gAudioRunning.load()) {
        int frequency = 0;
        {
            std::unique_lock<std::mutex> lock(gAudioMutex);
            gAudioCv.wait(lock, [] { return !gAudioRunning.load() || gHasPendingTone; });
            if (!gAudioRunning.load()) break;
            frequency = gPendingFrequency;
            gHasPendingTone = false;
        }

        auto buffer = makeToneBuffer(frequency, 22);
        WAVEHDR header{};
        header.lpData = reinterpret_cast<LPSTR>(buffer.data());
        header.dwBufferLength = static_cast<DWORD>(buffer.size() * sizeof(int16_t));

        waveOutPrepareHeader(device, &header, sizeof(header));
        waveOutWrite(device, &header, sizeof(header));
        while (gAudioRunning.load() && !(header.dwFlags & WHDR_DONE)) {
            Sleep(1);
        }
        waveOutUnprepareHeader(device, &header, sizeof(header));
    }

    waveOutReset(device);
    waveOutClose(device);
}

void startAudio() {
    gAudioRunning = true;
    gAudioThread = std::thread(audioLoop);
}

void stopAudio() {
    gAudioRunning = false;
    gAudioCv.notify_all();
    if (gAudioThread.joinable()) gAudioThread.join();
}

void playTone(int frequency) {
    frequency = clampInt(frequency, 37, 4096);
    {
        std::lock_guard<std::mutex> lock(gAudioMutex);
        gPendingFrequency = frequency;
        gHasPendingTone = true;
    }
    gAudioCv.notify_one();
}

void requestDraw() {
    if (gMainWindow) {
        InvalidateRect(gMainWindow, nullptr, FALSE);
    }
}

int visualStride() {
    int speed = gSpeed.load();
    if (speed >= 100) return 1000000000;
    if (speed >= 99) return 2000;
    if (speed >= 98) return 900;
    if (speed >= 97) return 450;
    if (speed >= 96) return 220;
    if (speed >= 95) return 60;
    if (speed >= 90) return 20;
    if (speed >= 80) return 5;
    return 1;
}

bool pauseStep(bool drew) {
    int speed = gSpeed.load();
    if (speed >= 96) {
        if (drew) {
            Sleep(speed >= 99 ? 0 : 1);
        }
        return !gAbort.load();
    }
    int delay = std::max(1, (100 - speed + 4) / 5);
    if (!drew) delay = 0;
    if (delay > 0) Sleep(static_cast<DWORD>(delay));
    return !gAbort.load();
}

bool showStep(int h1 = -1, int h2 = -1, int tone = -1) {
    ++gStepCounter;
    int speed = gSpeed.load();
    int stride = visualStride();
    bool frameCandidate = (gStepCounter % stride) == 0;
    bool drew = frameCandidate;
    auto now = std::chrono::steady_clock::now();

    if (drew) {
        std::lock_guard<std::mutex> lock(gArrayMutex);
        if (gActiveArray) {
            gArray = *gActiveArray;
        }
        gHighlight1 = h1;
        gHighlight2 = h2;
    }

    bool playedTone = drew && tone >= 0 &&
        (gLastToneTime.time_since_epoch().count() == 0 ||
         now - gLastToneTime >= std::chrono::milliseconds(speed >= 96 ? 30 : 10));
    if (playedTone) {
        playTone(tone);
        gLastToneTime = now;
    }
    if (drew) {
        gLastFrameTime = now;
        requestDraw();
    }
    return pauseStep(drew);
}

void clearHighlights() {
    std::lock_guard<std::mutex> lock(gArrayMutex);
    gHighlight1 = -1;
    gHighlight2 = -1;
}

void setEditInt(HWND edit, int value) {
    SetWindowTextW(edit, std::to_wstring(value).c_str());
}

void setScrollValue(HWND scroll, int value) {
    if (!scroll) return;
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_POS;
    info.nPos = value;
    SetScrollInfo(scroll, SB_CTL, &info, TRUE);
}

void configureScroll(HWND scroll, int minValue, int maxValue, int pageSize, int value) {
    SCROLLINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    info.nMin = minValue;
    info.nMax = maxValue;
    info.nPage = pageSize;
    info.nPos = value;
    SetScrollInfo(scroll, SB_CTL, &info, TRUE);
}

int readEditInt(HWND edit, int fallback) {
    wchar_t buffer[32]{};
    GetWindowTextW(edit, buffer, 31);
    try {
        return std::stoi(buffer);
    } catch (...) {
        return fallback;
    }
}

void randomiseArray(int requestedSize) {
    gAbort = false;
    gSize = clampInt(requestedSize, 2, kWidth - kBarAreaRightPadding);
    setEditInt(gSizeEdit, gSize);
    setScrollValue(gSizeScroll, gSize);

    std::vector<int> local(gSize);
    for (int i = 0; i < gSize; ++i) {
        local[i] = static_cast<int>((i / static_cast<double>(gSize)) * maxBarHeight());
    }

    std::mt19937 rng(std::random_device{}());
    int shuffleStride = 1;
    if (gSize >= 1000) shuffleStride = 80;
    else if (gSize >= 500) shuffleStride = 40;
    else if (gSize >= 250) shuffleStride = 20;
    else if (gSpeed.load() >= 95) shuffleStride = 10;

    {
        std::lock_guard<std::mutex> lock(gArrayMutex);
        gArray = local;
        gHighlight1 = -1;
        gHighlight2 = -1;
    }
    requestDraw();

    for (int i = gSize - 1; i > 0 && !gAbort.load(); --i) {
        std::uniform_int_distribution<int> dist(0, i);
        int j = dist(rng);
        std::swap(local[i], local[j]);
        if ((i % shuffleStride) == 0 || i == 1) {
            {
                std::lock_guard<std::mutex> lock(gArrayMutex);
                gArray = local;
            }
            showStep(i, j, toneForIndex(i, gSize));
        }
    }

    {
        std::lock_guard<std::mutex> lock(gArrayMutex);
        gArray = local;
    }
    clearHighlights();
    requestDraw();
}

void initialiseRandomArray(int requestedSize) {
    gSize = clampInt(requestedSize, 2, kWidth - kBarAreaRightPadding);
    setEditInt(gSizeEdit, gSize);
    setScrollValue(gSizeScroll, gSize);

    std::vector<int> local(gSize);
    for (int i = 0; i < gSize; ++i) {
        local[i] = static_cast<int>((i / static_cast<double>(gSize)) * maxBarHeight());
    }
    std::shuffle(local.begin(), local.end(), std::mt19937(std::random_device{}()));

    {
        std::lock_guard<std::mutex> lock(gArrayMutex);
        gArray = local;
        gHighlight1 = -1;
        gHighlight2 = -1;
    }
    requestDraw();
}

bool isSorted(const std::vector<int>& arr) {
    return std::is_sorted(arr.begin(), arr.end());
}

void bubbleSort(std::vector<int>& arr) {
    for (int i = 0; i < static_cast<int>(arr.size()) && !gAbort.load(); ++i) {
        for (int j = 0; j < static_cast<int>(arr.size()) - i - 1 && !gAbort.load(); ++j) {
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
                if (!showStep(j, j + 1, toneForIndex(j, static_cast<int>(arr.size())))) return;
            }
        }
    }
}

void selectionSort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    for (int i = 0; i < n && !gAbort.load(); ++i) {
        int minIdx = i;
        for (int j = i + 1; j < n && !gAbort.load(); ++j) {
            if (arr[j] < arr[minIdx]) minIdx = j;
        }
        std::swap(arr[i], arr[minIdx]);
        if (!showStep(i, minIdx, toneForIndex(i, n))) return;
    }
}

int quickPartition(std::vector<int>& arr, int low, int high) {
    int pivot = arr[high];
    int i = low - 1;
    for (int j = low; j < high && !gAbort.load(); ++j) {
        if (arr[j] <= pivot) {
            ++i;
            std::swap(arr[i], arr[j]);
            if (!showStep(i, j, toneForIndex(i, static_cast<int>(arr.size())))) return -1;
        }
    }
    std::swap(arr[i + 1], arr[high]);
    showStep(i + 1, high, toneForIndex(i + 1, static_cast<int>(arr.size())));
    return i + 1;
}

void quickSort(std::vector<int>& arr, int low, int high) {
    if (gAbort.load() || low >= high || isSorted(arr)) return;
    int pi = quickPartition(arr, low, high);
    if (pi < 0) return;
    quickSort(arr, low, pi - 1);
    quickSort(arr, pi + 1, high);
}

void pancakeSort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    for (int curr = n; curr > 1 && !gAbort.load(); --curr) {
        int maxIdx = static_cast<int>(std::max_element(arr.begin(), arr.begin() + curr) - arr.begin());
        if (maxIdx == curr - 1) continue;
        if (maxIdx != 0) {
            std::reverse(arr.begin(), arr.begin() + maxIdx + 1);
            if (!showStep(maxIdx, -1, toneForIndex(maxIdx, n))) return;
        }
        std::reverse(arr.begin(), arr.begin() + curr);
        if (!showStep(curr - 1, -1, toneForIndex(curr - 1, n))) return;
    }
}

bool heapify(std::vector<int>& arr, int n, int i) {
    if (gAbort.load()) return false;
    int largest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;
    if (left < n && arr[left] > arr[largest]) largest = left;
    if (right < n && arr[right] > arr[largest]) largest = right;
    if (largest != i) {
        std::swap(arr[i], arr[largest]);
        if (!showStep(i, largest, toneForIndex(i, static_cast<int>(arr.size())))) return false;
        return heapify(arr, n, largest);
    }
    return true;
}

void heapSort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    for (int i = n / 2 - 1; i >= 0 && !gAbort.load(); --i) {
        if (!heapify(arr, n, i)) return;
    }
    for (int i = n - 1; i > 0 && !gAbort.load(); --i) {
        std::swap(arr[0], arr[i]);
        if (!showStep(0, i, toneForIndex(0, n))) return;
        if (!heapify(arr, i, 0)) return;
    }
}

void merge(std::vector<int>& arr, int left, int mid, int right) {
    std::vector<int> l(arr.begin() + left, arr.begin() + mid + 1);
    std::vector<int> r(arr.begin() + mid + 1, arr.begin() + right + 1);
    int i = 0;
    int j = 0;
    int k = left;
    while (i < static_cast<int>(l.size()) && j < static_cast<int>(r.size()) && !gAbort.load()) {
        arr[k] = (l[i] <= r[j]) ? l[i++] : r[j++];
        if (!showStep(k, -1, toneForIndex(k, static_cast<int>(arr.size())))) return;
        ++k;
    }
    while (i < static_cast<int>(l.size()) && !gAbort.load()) {
        arr[k] = l[i++];
        if (!showStep(k, -1, toneForIndex(k, static_cast<int>(arr.size())))) return;
        ++k;
    }
    while (j < static_cast<int>(r.size()) && !gAbort.load()) {
        arr[k] = r[j++];
        if (!showStep(k, -1, toneForIndex(k, static_cast<int>(arr.size())))) return;
        ++k;
    }
}

void mergeSort(std::vector<int>& arr, int left, int right) {
    if (gAbort.load() || left >= right) return;
    int mid = left + (right - left) / 2;
    mergeSort(arr, left, mid);
    mergeSort(arr, mid + 1, right);
    merge(arr, left, mid, right);
}

void cocktailShakerSort(std::vector<int>& arr) {
    bool swapped = true;
    int start = 0;
    int end = static_cast<int>(arr.size()) - 1;
    while (swapped && !gAbort.load()) {
        swapped = false;
        for (int i = start; i < end && !gAbort.load(); ++i) {
            if (arr[i] > arr[i + 1]) {
                std::swap(arr[i], arr[i + 1]);
                swapped = true;
                if (!showStep(i, i + 1, toneForIndex(i, static_cast<int>(arr.size())))) return;
            }
        }
        if (!swapped) break;
        swapped = false;
        --end;
        for (int i = end - 1; i >= start && !gAbort.load(); --i) {
            if (arr[i] > arr[i + 1]) {
                std::swap(arr[i], arr[i + 1]);
                swapped = true;
                if (!showStep(i, i + 1, toneForIndex(i, static_cast<int>(arr.size())))) return;
            }
        }
        ++start;
    }
}

void radixSort(std::vector<int>& arr) {
    int maxVal = *std::max_element(arr.begin(), arr.end());
    for (int exp = 1; maxVal / exp > 0 && !gAbort.load(); exp *= 10) {
        std::vector<int> output(arr.size());
        int count[10]{};
        for (int value : arr) count[(value / exp) % 10]++;
        for (int i = 1; i < 10; ++i) count[i] += count[i - 1];
        for (int i = static_cast<int>(arr.size()) - 1; i >= 0; --i) {
            int digit = (arr[i] / exp) % 10;
            output[count[digit] - 1] = arr[i];
            count[digit]--;
        }
        for (int i = 0; i < static_cast<int>(arr.size()) && !gAbort.load(); ++i) {
            arr[i] = output[i];
            if (!showStep(i, -1, toneForValue(arr[i]))) return;
        }
        if (isSorted(arr)) return;
    }
}

void insertionSort(std::vector<int>& arr) {
    for (int i = 1; i < static_cast<int>(arr.size()) && !gAbort.load(); ++i) {
        int key = arr[i];
        int j = i - 1;
        while (j >= 0 && key < arr[j] && !gAbort.load()) {
            arr[j + 1] = arr[j];
            if (!showStep(j + 1, j, toneForValue(key))) return;
            --j;
        }
        arr[j + 1] = key;
        if (!showStep(j + 1, -1, toneForValue(key))) return;
    }
}

void sillySort(std::vector<int>& arr) {
    if (arr.size() < 2) return;

    std::mt19937 rng(std::random_device{}());
    int n = static_cast<int>(arr.size());
    int sillyRounds = std::max(200, n * 18);

    for (int round = 0; round < sillyRounds && !gAbort.load() && !isSorted(arr); ++round) {
        std::uniform_int_distribution<int> dist(0, n - 1);
        int a = dist(rng);
        int b = dist(rng);
        if (a == b) continue;
        if (a > b) std::swap(a, b);

        bool swapped = false;
        if (arr[a] > arr[b]) {
            std::swap(arr[a], arr[b]);
            swapped = true;
        } else if ((round % 17) == 0) {
            std::swap(arr[a], arr[b]);
            swapped = true;
        }

        if (swapped && !showStep(a, b, toneForIndex(a, n))) return;
    }

    // The silly part gets the array close-ish; this cleanup makes sure it actually finishes.
    bubbleSort(arr);
}

int calcMinRun(int n) {
    int r = 0;
    while (n >= 32) {
        r |= n & 1;
        n >>= 1;
    }
    return n + r;
}

void timSort(std::vector<int>& arr) {
    int n = static_cast<int>(arr.size());
    int minRun = calcMinRun(n);
    for (int start = 0; start < n && !gAbort.load(); start += minRun) {
        int end = std::min(start + minRun, n);
        for (int i = start + 1; i < end && !gAbort.load(); ++i) {
            int key = arr[i];
            int j = i - 1;
            while (j >= start && key < arr[j] && !gAbort.load()) {
                arr[j + 1] = arr[j];
                if (!showStep(j + 1, j, toneForValue(key))) return;
                --j;
            }
            arr[j + 1] = key;
        }
    }
    for (int size = minRun; size < n && !gAbort.load(); size *= 2) {
        for (int left = 0; left < n && !gAbort.load(); left += 2 * size) {
            int mid = std::min(n - 1, left + size - 1);
            int right = std::min(n - 1, left + 2 * size - 1);
            if (mid < right) merge(arr, left, mid, right);
        }
    }
}

void radixMergeSort(std::vector<int>& arr) {
    radixSort(arr);
    if (!gAbort.load() && !isSorted(arr)) {
        mergeSort(arr, 0, static_cast<int>(arr.size()) - 1);
    }
}

void radixHeapSort(std::vector<int>& arr) {
    radixSort(arr);
    if (!gAbort.load()) {
        heapSort(arr);
    }
}

using SortFunction = void (*)(std::vector<int>&);

void startWorker(std::function<void()> task) {
    if (gSorting.load()) return;
    if (gWorker.joinable()) gWorker.join();
    gAbort = false;
    gSorting = true;
    gWorker = std::thread([task]() {
        gStepCounter = 0;
        gLastFrameTime = {};
        gLastToneTime = {};
        task();
        clearHighlights();
        gSorting = false;
        requestDraw();
    });
}

void startSort(SortFunction sortFunction) {
    startWorker([sortFunction]() {
        std::vector<int> local;
        {
            std::lock_guard<std::mutex> lock(gArrayMutex);
            local = gArray;
        }
        gActiveArray = &local;
        sortFunction(local);
        gActiveArray = nullptr;
        {
            std::lock_guard<std::mutex> lock(gArrayMutex);
            gArray = local;
        }
    });
}

void startQuickSort() {
    startWorker([]() {
        std::vector<int> local;
        {
            std::lock_guard<std::mutex> lock(gArrayMutex);
            local = gArray;
        }
        gActiveArray = &local;
        if (!local.empty()) quickSort(local, 0, static_cast<int>(local.size()) - 1);
        gActiveArray = nullptr;
        {
            std::lock_guard<std::mutex> lock(gArrayMutex);
            gArray = local;
        }
    });
}

void createButton(HWND parent, const wchar_t* text, int id, int y) {
    CreateWindowW(
        L"BUTTON", text, WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
        kWidth - kButtonMargin - kButtonWidth, y, kButtonWidth, kButtonHeight,
        parent, reinterpret_cast<HMENU>(static_cast<intptr_t>(id)),
        GetModuleHandleW(nullptr), nullptr);
}

void drawArray(HDC hdc) {
    RECT client{};
    GetClientRect(gMainWindow, &client);
    RECT background{0, 0, client.right, client.bottom};
    HBRUSH white = CreateSolidBrush(kWhite);
    FillRect(hdc, &background, white);
    DeleteObject(white);

    std::vector<int> arr;
    int h1 = -1;
    int h2 = -1;
    {
        std::lock_guard<std::mutex> lock(gArrayMutex);
        arr = gArray;
        h1 = gHighlight1;
        h2 = gHighlight2;
    }

    HBRUSH blue = CreateSolidBrush(kBlue);
    HBRUSH green = CreateSolidBrush(kGreen);
    HBRUSH red = CreateSolidBrush(kRed);
    int width = barWidth();
    for (int i = 0; i < static_cast<int>(arr.size()); ++i) {
        HBRUSH brush = (i == h1) ? green : ((i == h2) ? red : blue);
        int value = clampInt(arr[i], 0, maxBarHeight());
        RECT rect{i * width, kHeight - value - kBarAreaBottomPadding, i * width + width, kHeight - kBarAreaBottomPadding};
        FillRect(hdc, &rect, brush);
    }
    DeleteObject(blue);
    DeleteObject(green);
    DeleteObject(red);
}

void releaseBackBuffer() {
    if (gBackBufferDc) {
        SelectObject(gBackBufferDc, gBackBufferOldBitmap);
        DeleteObject(gBackBufferBitmap);
        DeleteDC(gBackBufferDc);
    }
    gBackBufferDc = nullptr;
    gBackBufferBitmap = nullptr;
    gBackBufferOldBitmap = nullptr;
    gBackBufferWidth = 0;
    gBackBufferHeight = 0;
}

void ensureBackBuffer(HDC hdc, int width, int height) {
    if (gBackBufferDc && gBackBufferWidth == width && gBackBufferHeight == height) {
        return;
    }
    releaseBackBuffer();
    gBackBufferDc = CreateCompatibleDC(hdc);
    gBackBufferBitmap = CreateCompatibleBitmap(hdc, width, height);
    gBackBufferOldBitmap = SelectObject(gBackBufferDc, gBackBufferBitmap);
    gBackBufferWidth = width;
    gBackBufferHeight = height;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            int inputY = kHeight - kButtonHeight - kButtonMargin - 18;
            int scrollY = inputY + kButtonHeight + 2;
            gSizeEdit = CreateWindowW(L"EDIT", L"100", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER | ES_NUMBER,
                                      kButtonMargin, inputY,
                                      kButtonWidth, kButtonHeight, hwnd,
                                      reinterpret_cast<HMENU>(ID_SIZE), GetModuleHandleW(nullptr), nullptr);
            gSizeScroll = CreateWindowW(L"SCROLLBAR", nullptr, WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | SBS_HORZ,
                                        kButtonMargin, scrollY,
                                        kButtonWidth, 16, hwnd,
                                        reinterpret_cast<HMENU>(ID_SIZE_SCROLL), GetModuleHandleW(nullptr), nullptr);
            gSpeedEdit = CreateWindowW(L"EDIT", L"90", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | WS_BORDER | ES_NUMBER,
                                       2 * kButtonMargin + kButtonWidth, inputY,
                                       kButtonWidth, kButtonHeight, hwnd,
                                       reinterpret_cast<HMENU>(ID_SPEED), GetModuleHandleW(nullptr), nullptr);
            gSpeedScroll = CreateWindowW(L"SCROLLBAR", nullptr, WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | SBS_HORZ,
                                         2 * kButtonMargin + kButtonWidth, scrollY,
                                         kButtonWidth, 16, hwnd,
                                         reinterpret_cast<HMENU>(ID_SPEED_SCROLL), GetModuleHandleW(nullptr), nullptr);
            configureScroll(gSizeScroll, 2, kWidth - kBarAreaRightPadding, 1, gSize);
            configureScroll(gSpeedScroll, 0, 100, 1, gSpeed.load());
            CreateWindowW(L"BUTTON", L"Shuffle", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
                          kWidth - kButtonMargin - kButtonWidth, kHeight - kButtonHeight - kButtonMargin,
                          kButtonWidth, kButtonHeight, hwnd,
                          reinterpret_cast<HMENU>(ID_SHUFFLE), GetModuleHandleW(nullptr), nullptr);
            gStopButton = CreateWindowW(L"BUTTON", L"Stop", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON,
                                        3 * kButtonMargin + 2 * kButtonWidth,
                                        kHeight - kButtonHeight - kButtonMargin,
                                        kButtonWidth, kButtonHeight, hwnd,
                                        reinterpret_cast<HMENU>(ID_STOP), GetModuleHandleW(nullptr), nullptr);

            int y = kButtonMargin;
            createButton(hwnd, L"Bubble Sort", ID_BUBBLE, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Selection Sort", ID_SELECTION, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Quick Sort", ID_QUICK, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Heap Sort", ID_HEAP, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Pancake Sort", ID_PANCAKE, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Merge Sort", ID_MERGE, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Cocktail Shaker", ID_COCKTAIL, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Radix Sort", ID_RADIX, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Insertion Sort", ID_INSERTION, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Tim Sort", ID_TIM, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Radix Merge", ID_RADIX_MERGE, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Radix Heap", ID_RADIX_HEAP, y); y += kButtonHeight + kButtonMargin;
            createButton(hwnd, L"Silly Sort", ID_SILLY, y);

            initialiseRandomArray(gSize);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_SIZE && HIWORD(wParam) == EN_KILLFOCUS) {
                gSize = clampInt(readEditInt(gSizeEdit, gSize), 2, kWidth - kBarAreaRightPadding);
                setEditInt(gSizeEdit, gSize);
                setScrollValue(gSizeScroll, gSize);
            }
            if (id == ID_SPEED && HIWORD(wParam) == EN_KILLFOCUS) {
                gSpeed = clampInt(readEditInt(gSpeedEdit, gSpeed.load()), 0, 100);
                setEditInt(gSpeedEdit, gSpeed.load());
                setScrollValue(gSpeedScroll, gSpeed.load());
            }
            if (id == ID_STOP && HIWORD(wParam) == BN_CLICKED) {
                gAbort = true;
                return 0;
            }
            if (HIWORD(wParam) != BN_CLICKED || gSorting.load()) return 0;

            gSpeed = clampInt(readEditInt(gSpeedEdit, gSpeed.load()), 0, 100);
            setEditInt(gSpeedEdit, gSpeed.load());

            switch (id) {
                case ID_SHUFFLE:
                    startWorker([]() { randomiseArray(readEditInt(gSizeEdit, gSize)); });
                    break;
                case ID_BUBBLE: startSort(bubbleSort); break;
                case ID_SELECTION: startSort(selectionSort); break;
                case ID_QUICK: startQuickSort(); break;
                case ID_HEAP: startSort(heapSort); break;
                case ID_PANCAKE: startSort(pancakeSort); break;
                case ID_MERGE:
                    startWorker([]() {
                        std::vector<int> local;
                        {
                            std::lock_guard<std::mutex> lock(gArrayMutex);
                            local = gArray;
                        }
                        gActiveArray = &local;
                        if (!local.empty()) mergeSort(local, 0, static_cast<int>(local.size()) - 1);
                        gActiveArray = nullptr;
                        {
                            std::lock_guard<std::mutex> lock(gArrayMutex);
                            gArray = local;
                        }
                    });
                    break;
                case ID_COCKTAIL: startSort(cocktailShakerSort); break;
                case ID_RADIX: startSort(radixSort); break;
                case ID_INSERTION: startSort(insertionSort); break;
                case ID_TIM: startSort(timSort); break;
                case ID_RADIX_MERGE: startSort(radixMergeSort); break;
                case ID_RADIX_HEAP: startSort(radixHeapSort); break;
                case ID_SILLY: startSort(sillySort); break;
            }
            return 0;
        }
        case WM_HSCROLL: {
            HWND scroll = reinterpret_cast<HWND>(lParam);
            if (scroll != gSizeScroll && scroll != gSpeedScroll) {
                break;
            }

            SCROLLINFO info{};
            info.cbSize = sizeof(info);
            info.fMask = SIF_ALL;
            GetScrollInfo(scroll, SB_CTL, &info);

            int position = info.nPos;
            int line = scroll == gSizeScroll ? 1 : 1;
            int page = scroll == gSizeScroll ? 25 : 5;

            switch (LOWORD(wParam)) {
                case SB_LINELEFT: position -= line; break;
                case SB_LINERIGHT: position += line; break;
                case SB_PAGELEFT: position -= page; break;
                case SB_PAGERIGHT: position += page; break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION: position = info.nTrackPos; break;
                case SB_LEFT: position = info.nMin; break;
                case SB_RIGHT: position = info.nMax; break;
                default: return 0;
            }

            if (scroll == gSizeScroll) {
                gSize = clampInt(position, 2, kWidth - kBarAreaRightPadding);
                setEditInt(gSizeEdit, gSize);
                setScrollValue(gSizeScroll, gSize);
            } else {
                gSpeed = clampInt(position, 0, 100);
                setEditInt(gSpeedEdit, gSpeed.load());
                setScrollValue(gSpeedScroll, gSpeed.load());
            }
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (gSorting.load()) {
                    gAbort = true;
                } else {
                    PostMessageW(hwnd, WM_CLOSE, 0, 0);
                }
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client{};
            GetClientRect(hwnd, &client);
            int bufferWidth = client.right - client.left;
            int bufferHeight = client.bottom - client.top;

            ensureBackBuffer(hdc, bufferWidth, bufferHeight);
            drawArray(gBackBufferDc);
            BitBlt(hdc, 0, 0, bufferWidth, bufferHeight, gBackBufferDc, 0, 0, SRCCOPY);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            gAbort = true;
            if (gWorker.joinable()) gWorker.join();
            stopAudio();
            releaseBackBuffer();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    startAudio();

    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"TheoSortingCppWindow";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    RECT rect{0, 0, kWidth, kHeight};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    gMainWindow = CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Theo's Sorting app - C++",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!gMainWindow) return 1;

    ShowWindow(gMainWindow, showCommand);
    UpdateWindow(gMainWindow);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

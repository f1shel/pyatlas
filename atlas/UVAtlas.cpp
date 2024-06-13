#if __cplusplus < 201703L
#error Requires C++17 (and /Zc:__cplusplus with MSVC)
#endif

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "interface.h"

#include <iostream>
#include <fstream>
#include <cstdint>
#include <ctime>
#include <memory>
#include <system_error>
#include <tuple>

using namespace DirectX;
using namespace std;
using namespace Eigen;

enum CHANNELS
{
    CHANNEL_NONE = 0,
    CHANNEL_NORMAL,
    CHANNEL_COLOR,
    CHANNEL_TEXCOORD,
};

// uint64_t GetTickCount64() {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts); // 使用 CLOCK_MONOTONIC 获取自系统启动以来的时间
//     return static_cast<uint64_t>(ts.tv_sec) * 1000 + static_cast<uint64_t>(ts.tv_nsec) / 1000000;
// }

// HRESULT __cdecl UVAtlasCallback(float fPercentDone)
// {
//     static ULONGLONG s_lastTick = 0;
//     const ULONGLONG tick = GetTickCount64();
//     if ((tick - s_lastTick) > 500)
//     {
//         printf("\r%.2f%%   ", double(fPercentDone) * 100);
//         s_lastTick = tick;
//     }
//     if (c_kbhit())
//     {
//         if (c_getch() == 27)
//         {
//             printf("*** ABORT ***");
//             return E_ABORT;
//         }
//     }
//     if (fPercentDone > 1.0 - 1e-4f)
//         printf("\n");
//     return S_OK;
// }

// void err_msg(HRESULT hr) {
//     std::string message = std::system_category().message(hr);
//     std::cerr << message << std::endl;
//     exit(1);
// }

#define RETURN_EMPTY { \
    VectorXi a; \
    Matrix<int, -1, -1, RowMajor> b; \
    Matrix<float, -1, -1, RowMajor> c; \
    return std::tie(a, b, c); }

std::tuple<
    Eigen::VectorXi,
    Eigen::Matrix<int, -1, -1, Eigen::RowMajor>,
    Eigen::Matrix<float, -1, -1, Eigen::RowMajor>
>
atlas(
    const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>& vertices,
    const Eigen::Matrix<int, -1, -1, Eigen::RowMajor>& faces,
    size_t maxCharts,
    float maxStretch,
    float gutter,
    size_t width,
    size_t height
) {
    std::vector<XMFLOAT3> positions(vertices.rows());
    std::vector<uint32_t> indices(faces.size());
    memcpy((void*)positions.data(), (void*)vertices.data(), sizeof(float) * vertices.size());
    memcpy((void*)indices.data(), (void*)faces.data(), sizeof(uint32_t) * faces.size());

    CHANNELS perVertex = CHANNEL_NONE;
    UVATLAS uvOptions = UVATLAS_DEFAULT;
    UVATLAS uvOptionsEx = UVATLAS_DEFAULT;

    // std::cout << "UVAtlasTool: " << "generating adjacency" << std::endl;
    size_t nVertsOriginal = positions.size();
    size_t nFacesOriginal = indices.size() / 3;
    size_t nVerts = nVertsOriginal, nFaces = nFacesOriginal;
    std::vector<uint32_t> adjacency;
    adjacency.resize(nFaces * 3);

    // Adjacency
    const float epsilon = 0.f;
    HRESULT hr = DirectX::GenerateAdjacencyAndPointReps(indices.data(), nFaces, positions.data(), nVerts, epsilon, nullptr, adjacency.data());
    if (FAILED(hr)) {
        std::cerr << "failed to generate adjacency" << std::endl;
        RETURN_EMPTY;
    }

    std::vector<uint32_t> dups{};
    // std::cout << "UVAtlasTool: " << "validating" << std::endl;
    std::wstring msgs;
    hr = DirectX::Validate(indices.data(), nFacesOriginal, nVertsOriginal, adjacency.data(), VALIDATE_BACKFACING | VALIDATE_BOWTIES, &msgs);
    if (!msgs.empty())
    {
        wprintf(L"\nWARNING: \n");
        wprintf(L"%ls", msgs.c_str());
    }
    if (FAILED(hr)) {
        std::cerr << "failed to validate" << std::endl;
        RETURN_EMPTY;
    }

    // Clean
    // std::cout << "UVAtlasTool: " << "cleaning" << std::endl;
    hr = DirectX::Clean(indices.data(), nFacesOriginal, nVertsOriginal, adjacency.data(), nullptr, dups, true);
    std::vector<XMFLOAT3> cleaned_positions = positions;
    if (FAILED(hr)) {
        std::cerr << "failed to clean" << std::endl;
        RETURN_EMPTY;
    }
    if (!dups.empty()) {
        const size_t nNewVerts = nVertsOriginal + dups.size();
        size_t j = nVertsOriginal;
        std::vector<XMFLOAT3> pos{};
        pos.resize(nNewVerts);
        for (auto it = dups.begin(); it != dups.end() && (j < nNewVerts); ++it, ++j)
        {
            if(*it < nVertsOriginal) {
                std::cerr << *it << ", " << nVertsOriginal << std::endl;
                exit(1);
            }

            pos[j] = positions[*it];
        }
        cleaned_positions = pos;
        nVerts = nNewVerts;
    }

    std::vector<float> IMTData{};

    // std::cout << "UVAtlasTool: " << "Computing isochart atlas on mesh" << std::endl;
    std::vector<UVAtlasVertex> vb;
    std::vector<uint8_t> ib;
    float outStretch = 0.f;
    size_t outCharts = 0;
    std::vector<uint32_t> facePartitioning;
    std::vector<uint32_t> vertexRemapArray;
    hr = UVAtlasCreate(cleaned_positions.data(), nVerts,
        indices.data(), DXGI_FORMAT_R32_UINT, nFaces,
        maxCharts, maxStretch, width, height, gutter,
        adjacency.data(), nullptr,
        nullptr,
        nullptr, // UVAtlasCallback,
        UVATLAS_DEFAULT_CALLBACK_FREQUENCY,
        uvOptions | uvOptionsEx, vb, ib,
        &facePartitioning,
        &vertexRemapArray,
        &outStretch, &outCharts);
    if (FAILED(hr)) {
        std::cerr << "failed to create atlas" << std::endl;
        RETURN_EMPTY;
    }
    printf("Output # of charts: %zu, resulting stretching %f, %zu verts\n", outCharts, double(outStretch), vb.size());

    // std::cout << "UVAtlasTool: " << "updating faces" << std::endl;
    assert((ib.size() / sizeof(uint32_t)) == (nFaces * 3));
    assert(facePartitioning.size() == nFaces);
    assert(vertexRemapArray.size() == vb.size());
    nVerts = vb.size();

    std::vector<XMFLOAT2> texcoord{};
    texcoord.resize(nVerts);
    auto txptr = texcoord.data();
    size_t j = 0;
    for (auto it = vb.cbegin(); it != vb.cend() && j < nVerts; ++it, ++txptr)
        *txptr = it->uv;

    Map<VectorXi> remapping(reinterpret_cast<int*>(vertexRemapArray.data()), nVerts, 1);
    Map<Matrix<int, -1, -1, RowMajor>> new_indices(reinterpret_cast<int*>(ib.data()), nFaces, 3);
    Map<Matrix<float, -1, -1, RowMajor>> uvs(reinterpret_cast<float*>(texcoord.data()), nVerts, 2);
    
    return std::tie(remapping, new_indices, uvs);
}

int main() {return 0;}
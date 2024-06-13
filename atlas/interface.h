#include <Eigen/Dense>
#include "DirectXMath.h"
#include "DirectXMesh.h"
#include "UVAtlas.h"
#include "conio.h"

#include <iostream>
#include <fstream>
#include <cstdint>
#include <ctime>
#include <memory>
#include <system_error>

std::tuple<
    Eigen::VectorXi,
    Eigen::Matrix<int, -1, -1, Eigen::RowMajor>,
    Eigen::Matrix<float, -1, -1, Eigen::RowMajor>>
atlas(
    const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>& vertices,
    const Eigen::Matrix<int, -1, -1, Eigen::RowMajor>& faces,
    size_t maxCharts = 0,
    float maxStretch = 0.16667f,
    float gutter = 2.f,
    size_t width = 512,
    size_t height = 512
);
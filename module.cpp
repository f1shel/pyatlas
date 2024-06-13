#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>
#include "atlas/interface.h"

namespace py = pybind11;

PYBIND11_MODULE(pyatlas, m) {
    m.doc() = "pybind11 pyatlas module";
    m.def("atlas", &atlas,
          py::arg("vertices"),
          py::arg("faces"),
          py::arg("maxCharts") = 0,
          py::arg("maxStretch") = 0.16667,
          py::arg("gutter") = 2.0,
          py::arg("width") = 512,
          py::arg("height") = 512,
          "creating atlas"
    );
}
// File: tint_sdl.cpp
// Author: Kyle Lukaszek
// Email: kylelukaszek [at] gmail [dot] com
// Date: 2024-09-26
// -------------------------------------------------------------
// Special thanks to the Tint contributors for their work on the Tint compiler.
// -------------------------------------------------------------
//
// License: Apache 2.0 (Follows the same license as Tint)
//
// Description:
// This file is a simple C++ wrapper around the Tint compiler for converting SPIR-V to WGSL.
// The main function is tint_spv_to_wgsl which takes in a SPIR-V shader as a uint8_t array from
// SDL and returns a char* to the WGSL shader.
//
// This .cpp file is meant to be assigned as an extension to libtint.a which can be found
// at https://www.github.com/klukaszek/tint-wasm.
//
// This has already been done to the included libtint_wasm.a file in this directory.
//
// MAKING CHANGES:
//
// If you make any changes to this file, it is important that we update the object files
// in the libtint_wasm.a static library.
//
// To update libtint_wasm.a, you can use the following command:
// ->  em++ -sWASM=0 -I./<TINT_WASM> tint_sdl.cpp -L.<TINT_WASM> -ltint -c
// ->  cp <TINT_WASM>/libtint.a ./libtint_wasm.a
// ->  emar rcs libtint_wasm.a tint_sdl.o
//
// Where <TINT_WASM> is the path to the tint-wasm repo with the precompiled static library.
//
// The static library will already be included with SDL3 WebGPU along with the tint_sdl.cpp file.
//
// If you make any changes to this file, it is important to update libtint_wasm.a using the emar
// command above. This will overwrite the existing symbols with the new ones.
// ---------------------------------------------------------------

#include "api/tint.h"
#include "lang/wgsl/common/allowed_features.h"
#include "spirv-tools/libspirv.h"
#include "utils/diagnostic/formatter.h"
#include <sys/types.h>
#define TINT_BUILD_WGSL_WRITER 1
#define TINT_BUILD_WGSL_READER 1
#define TINT_BUILD_SPV_READER  1
#define TINT_BUILD_SPV_WRITER  1

#include "cmd/common/helper.h"
#include "spirv-tools/libspirv.hpp"
#include "tint.h"
#include "utils/diagnostic/source.h"
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <memory>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

std::vector<uint32_t> convertUint8ArrayToUint32Array(const uint8_t *data, size_t size)
{
    std::vector<uint32_t> result;
    result.reserve((size / 4));
    for (size_t i = 0; i < size; i += 4) {
        uint32_t word = 0;
        word |= data[i + 0] << 0;
        word |= data[i + 1] << 8;
        word |= data[i + 2] << 16;
        word |= data[i + 3] << 24;
        result.push_back(word);
    }
    return result;
}

tint::cmd::ProgramInfo LoadProgramInfo(const tint::cmd::LoadProgramOptions &opts, const std::vector<uint32_t> shader_code)
{
    auto load = [&]() -> tint::cmd::ProgramInfo {
        return tint::cmd::ProgramInfo{
            /* program */ tint::spirv::reader::Read(shader_code, {}),
            /* source_file */ nullptr,
        };
    };

    tint::cmd::ProgramInfo info = load();

    if (info.program.Diagnostics().Count() > 0) {

        std::cerr << info.program.Diagnostics().Str() << std::endl;

        fflush(stderr);
    }

    if (!info.program.IsValid()) {
        std::cerr << "Program is not valid." << std::endl;
        return info;
    }

    return info;
}

extern "C" {

void tint_initialize(void)
{
    tint::Initialize();
}

char *tint_spv_to_wgsl(const uint8_t *shader_data, const size_t shader_size)
{
    tint::cmd::LoadProgramOptions opts;

    opts.filename = "spv-shader";
    opts.use_ir = false;
    opts.printer = nullptr;

    // Properly convert SDL's uint8_t SPIRV array to a uint32_t array
    std::vector<uint32_t> shader_code = convertUint8ArrayToUint32Array(shader_data, shader_size);

    auto info = LoadProgramInfo(opts, shader_code);

    tint::wgsl::writer::Options options;
    auto result = tint::wgsl::writer::Generate(info.program, options);

    // Malloc the "str" pointer to the size of the WGSL string
    char *wgsl = (char *)malloc(result->wgsl.size() + 1);

    // Copy the WGSL string to the provided buffer
    std::strcpy(wgsl, result->wgsl.c_str());

    return wgsl;
}
}

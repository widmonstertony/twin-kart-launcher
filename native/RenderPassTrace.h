#pragma once
#include <d3d11.h>
#include <cstddef>
#include <cstdint>

namespace TwinKartRender {
// Bytecode is hashed once when a shader is created; only the digest is logged.
bool TagShader(ID3D11DeviceChild* shader, const void* bytecode, size_t length);
bool ShaderHash(ID3D11DeviceChild* shader,char (&hex)[65]);
bool ShaderThreadGroup(ID3D11ComputeShader*,unsigned& x,unsigned& y,unsigned& z);
// Reads bound state, balances every Get* reference and never submits GPU work.
// Returns zero for a non-scene pass, unsupported context or insufficient buffer.
size_t DescribePass(ID3D11DeviceContext* context, bool compute, unsigned frame,
    uintptr_t callerRva, char* output, size_t capacity);
}

#version 430 core

layout(local_size_x = 128, local_size_y = 1) in;

uniform layout(binding = 0, offset = 0) atomic_uint u_counter;
layout(std430, binding = 1) coherent volatile buffer A { volatile uint val[];   } ssbo;

void main()
{
  uint spot = atomicCounterIncrement(u_counter);
  ssbo.val[spot] = spot;
}

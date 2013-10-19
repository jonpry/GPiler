/* 
GPiler - example1.cu
Copyright (C) 2013 Jon Pry and Charles Cooper

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
 
#include <stdio.h>
#include <cuda.h>

//Place holder function so we can redirect the call during link 
__device__  __noinline__ void mapitfoo(int N, int *out, int *in){
	out[N] = in[N] * in[N];
}

// Kernel that executes on the CUDA device
__global__ void square_array(int N, int *out, int *in)
{
  for(int i=0; i<N; i+=blockDim.x*gridDim.x){
  	int idx = i + blockIdx.x * blockDim.x + threadIdx.x;
	if (idx<N)
		mapitfoo(idx,out,in);
  }
}
 
// main routine that executes on the host
int main(void)
{
  int *in_h, *in_d, *out_h, *out_d;  // Pointer to host & device arrays
  const int N = 10;  // Number of elements in arrays


  size_t size = N * sizeof(int);
  in_h = (int *)malloc(size);        // Allocate array on host
  out_h = (int *)malloc(size);        // Allocate array on host

  cudaMalloc((void **) &in_d, size);   // Allocate array on device
  cudaMalloc((void **) &out_d, size);   // Allocate array on device


  // Initialize host array and copy it to CUDA device
  for (int i=0; i<N; i++) in_h[i] = i;
  cudaMemcpy(in_d, in_h, size, cudaMemcpyHostToDevice);

  // Do calculation on device:
  int block_size = 32; //TODO: get this number from device info
  int n_blocks = 1; //TODO: determine number of SM's
  square_array <<< n_blocks, block_size >>> (N, out_d, in_d);
  // Retrieve result from device and store it in host array

  cudaMemcpy(out_h, out_d, sizeof(int)*N, cudaMemcpyDeviceToHost);
  
  // Print results
  for (int i=0; i<N; i++) printf("%d %d %d\n", i, in_h[i], out_h[i]);
  // Cleanup
  free(in_h); 
  free(out_h);
  cudaFree(in_d);
  cudaFree(out_d);
}

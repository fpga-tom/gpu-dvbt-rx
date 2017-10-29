#include <iostream>
#include <iterator>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <ctime>

#include "viterbi.h"

#define NUM_STATES 64
#define NUM_INPUT_SYMBOLS 2
#define NUM_OUTPUT_SYMBOLS 4
#define NUM_BLOCKS (189)
#define TRACEBACK (128)
#define NUM_H 36288
#define NUM_D_BLOCKS 378
#define NUM_D_THREADS 96
#define NUM_D_BITS 756
#define NUM_L (7*5)
#define BLOCK_LEN (TRACEBACK + 2*NUM_L)

__constant__ const char outputs[NUM_STATES][NUM_INPUT_SYMBOLS] = { { 0, 3 }, {
		3, 0 }, { 1, 2 }, { 2, 1 }, { 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 }, { 3,
		0 }, { 0, 3 }, { 2, 1 }, { 1, 2 }, { 3, 0 }, { 0, 3 }, { 2, 1 },
		{ 1, 2 }, { 3, 0 }, { 0, 3 }, { 2, 1 }, { 1, 2 }, { 3, 0 }, { 0, 3 }, {
				2, 1 }, { 1, 2 }, { 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 },
		{ 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 }, { 2, 1 }, { 1, 2 }, { 3, 0 }, {
				0, 3 }, { 2, 1 }, { 1, 2 }, { 3, 0 }, { 0, 3 }, { 1, 2 },
		{ 2, 1 }, { 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 }, { 0, 3 }, { 3, 0 }, {
				1, 2 }, { 2, 1 }, { 0, 3 }, { 3, 0 }, { 1, 2 }, { 2, 1 },
		{ 0, 3 }, { 3, 0 }, { 2, 1 }, { 1, 2 }, { 3, 0 }, { 0, 3 }, { 2, 1 }, {
				1, 2 }, { 3, 0 }, { 0, 3 }, };

typedef struct {
	unsigned int w;
	unsigned char prev;
} pm_t;

__global__ void calc_bm(char *encData,
		pm_t (*pm)[NUM_BLOCKS][BLOCK_LEN][NUM_STATES]) {
	int state = threadIdx.x;
	int block = blockIdx.x;

	int prevState = (state & (~32U)) << 1;
	int prevSymbol = (state >> 5) & 1;

	int offset = block * (BLOCK_LEN);
	unsigned int pm0 = 0;
	unsigned int pm1 = 0;

	for (int tms = 0; tms < (BLOCK_LEN); tms++) {
		int _tms = tms + offset;

		unsigned char data = encData[_tms];
		unsigned char en0 = (~data >> 2) & 1;
		unsigned char en1 = (~data >> 3) & 1;
		unsigned char d0 = (data ^ outputs[prevState][prevSymbol]);
		unsigned char d1 = (data ^ outputs[prevState + 1][prevSymbol]);

		unsigned int c0 = ((d0 >> 1) & en1) + (d0 & en0);
		unsigned int c1 = ((d1 >> 1) & en1) + (d1 & en0);

		unsigned int w0 = c0 + pm0;
		unsigned int w1 = c1 + pm1;

		(*pm)[block][tms][state] = {
			.w = w0 < w1 ? w0 : w1,
			.prev = w0 < w1 ? (unsigned char)(prevState) : (unsigned char)(prevState + 1)
		};

		__syncthreads();

		pm0 = (*pm)[block][tms][prevState].w;
		pm1 = (*pm)[block][tms][prevState + 1].w;
	}

}

__global__ void merge(pm_t (*pm)[NUM_BLOCKS][TRACEBACK][NUM_STATES]) {
	int state = threadIdx.x;

	for (int block = 1; block < NUM_BLOCKS; block++) {
		unsigned int w0 = 0;
		unsigned int w1 = 0;

		int prevState = (state & (~32U)) << 1;
		if (block > 0) {
			w0 = (*pm)[block - 1][TRACEBACK - 1][prevState].w;
			w1 = (*pm)[block - 1][TRACEBACK - 1][prevState + 1].w;
			(*pm)[block][TRACEBACK - 1][state].w += w0 < w1 ? w0 : w1;
		}

		__syncthreads();
	}

}

__global__ void traceback(char *decData,
		pm_t (*pm)[NUM_BLOCKS][BLOCK_LEN][NUM_STATES]) {

	int block = blockIdx.x;
	int offset = block * (TRACEBACK);

	unsigned int minIdx = 0;
//	unsigned int min = 0xffffffff;
//	int tms = BLOCK_LEN - 1;
//	for (int i = 0; i < NUM_STATES; i++) {
//		unsigned int w = (*pm)[block][tms][i].w;
//		if (min > w) {
//			min = w;
//			minIdx = i;
//		}
//	}

	for (int _tms = BLOCK_LEN - 1; _tms >= BLOCK_LEN - NUM_L; _tms--) {
			minIdx = (*pm)[block][_tms][minIdx].prev;
	}

	for (int _tms = BLOCK_LEN - NUM_L - 1; _tms >= NUM_L; _tms--) {
		decData[_tms + offset - NUM_L] = (minIdx >> 5) & 1;
		minIdx = (*pm)[block][_tms][minIdx].prev;
	}
}

__global__ void deint_symbol(const char* bitset, char* tmp, int frame,
		int* deint_h) {
	int it = threadIdx.x;
	int block = blockIdx.x;
	int offset = block * NUM_D_THREADS + it;

	if (frame % 2 == 0) {
		tmp[offset] = bitset[deint_h[offset]];
	} else {
		tmp[deint_h[offset]] = bitset[offset];
	}
}

__global__ void deint_bit(const char* tmp, char* result, int* bit_table) {
	int inner = threadIdx.x;
	int outer = blockIdx.x;
	result[bit_table[inner] - 1 + outer * 756] = tmp[inner + outer * 756];
}

static pm_t (*pm)[NUM_BLOCKS][BLOCK_LEN][NUM_STATES];
static char *encData;
static char *decData;
static char *deint_tmp;
static char *deint_data;
static char *deint_output;
static int *deint_h;
static int *deint_bit_table;

cudaStream_t viterbi_stream;
cudaStream_t deint_stream;

void gpu_deint_init() {
	cudaStreamCreate(&deint_stream);
	cudaMalloc(&deint_tmp, NUM_D_BLOCKS * NUM_D_THREADS);
	cudaMalloc(&deint_data, NUM_D_BLOCKS * NUM_D_THREADS);
	cudaMalloc(&deint_output, NUM_D_BLOCKS * NUM_D_THREADS);
	cudaMalloc(&deint_h, NUM_D_BLOCKS * NUM_D_THREADS * sizeof(int));
	cudaMalloc(&deint_bit_table, NUM_D_BITS * sizeof(int));

	cudaMemcpy(deint_h, H, NUM_D_THREADS * NUM_D_BLOCKS * sizeof(int),
			cudaMemcpyHostToDevice);
	cudaMemcpy(deint_bit_table, bit_table, NUM_D_BITS * sizeof(int),
			cudaMemcpyHostToDevice);
}

void gpu_deint_free() {
	cudaStreamDestroy(deint_stream);
	cudaFree(deint_tmp);
	cudaFree(deint_data);
	cudaFree(deint_output);
	cudaFree(deint_h);
	cudaFree(deint_bit_table);
}

void gpu_viterbi_init() {
	cudaStreamCreate(&viterbi_stream);
	cudaMalloc(&pm,
	NUM_BLOCKS * (BLOCK_LEN) * NUM_STATES * sizeof(pm_t));
	cudaMalloc(&encData, (BLOCK_LEN) * NUM_BLOCKS);
	cudaMalloc(&decData, TRACEBACK * NUM_BLOCKS);
}

void gpu_viterbi_free() {
	cudaFree(pm);
	cudaFree(encData);
	cudaFree(decData);
	cudaStreamDestroy(viterbi_stream);
}

void gpu_viterbi_decode(const char* data, char* output) {

	static char encData_tmp[(BLOCK_LEN) * NUM_BLOCKS] = {0,};
	char decData_tmp[TRACEBACK * NUM_BLOCKS];

	for (int b = 0; b < NUM_BLOCKS; b++) {
		for (int i = 0; i < TRACEBACK + NUM_L; i++) {

			int d = (data[(b * TRACEBACK + i) * 2] & 1) << 1
					| (data[(b * TRACEBACK + i) * 2 + 1] & 1);
			d |= (data[(b * TRACEBACK + i) * 2] & 2) << 2
					| (data[(b * TRACEBACK + i) * 2 + 1] & 2) << 1;

			encData_tmp[b * (BLOCK_LEN) + i + NUM_L] = d;
		}
		if (b > 0) {
//			memcpy(&encData_tmp[(b-1)*BLOCK_LEN + TRACEBACK + NUM_L], &encData_tmp[b*BLOCK_LEN + NUM_L], NUM_L);
			memcpy(&encData_tmp[(b) * (BLOCK_LEN)], &encData_tmp[(b - 1)
						* (BLOCK_LEN) + TRACEBACK], NUM_L);
		}
	}

	cudaMemcpy(encData, encData_tmp, (BLOCK_LEN) * NUM_BLOCKS,
			cudaMemcpyHostToDevice);
	calc_bm<<<dim3(NUM_BLOCKS), dim3(NUM_STATES), 0, viterbi_stream>>>(encData,
			pm);
	cudaStreamSynchronize(viterbi_stream);
//	merge<<<dim3(1), dim3(NUM_STATES), 0, viterbi_stream>>>(pm);
//	cudaStreamSynchronize(viterbi_stream);
	traceback<<<NUM_BLOCKS, 1, 0, viterbi_stream>>>(decData, pm);
	cudaStreamSynchronize(viterbi_stream);

	cudaMemcpy(decData_tmp, decData, TRACEBACK * NUM_BLOCKS,
			cudaMemcpyDeviceToHost);

	memcpy(encData_tmp, &encData_tmp[(NUM_BLOCKS-1)*BLOCK_LEN + TRACEBACK], NUM_L);

	int count = 0;
	for (int i = 0; i < TRACEBACK * NUM_BLOCKS; i += 8) {
		output[i >> 3] = 0;
		for (int j = 7; j >= 0; j--) {
			output[i >> 3] |= (decData_tmp[count++] & 1) << j;
		}
	}

}

void gpu_deinterleave(const char* data, char* output, int frame) {

	cudaMemcpy(deint_data, data, NUM_D_THREADS * NUM_D_BLOCKS,
			cudaMemcpyHostToDevice);

	deint_symbol<<<NUM_D_BLOCKS, NUM_D_THREADS, 0, deint_stream>>>(deint_data,
			deint_tmp, frame, deint_h);
	cudaStreamSynchronize(deint_stream);
	deint_bit<<<48, 756, 0, deint_stream>>>(deint_tmp, deint_output,
			deint_bit_table);
	cudaStreamSynchronize(deint_stream);

	cudaMemcpy(output, deint_output, NUM_D_THREADS * NUM_D_BLOCKS,
			cudaMemcpyDeviceToHost);
}

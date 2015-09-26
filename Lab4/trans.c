/* Name:      Ti-Fen Pan
 * Andrew ID: tpan
 *
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"
#include "contracts.h"

#define BLOCK_SIZE 8

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_64(int A[64][64], int B[64][64]);
/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. The REQUIRES and ENSURES from 15-122 are included
 *     for your convenience. They can be removed if you like.
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    REQUIRES(M > 0);
    REQUIRES(N > 0);
    int i, j, tmp, i1, j1, diag_idx;
    int b_sz; /*block size*/

    if(N == 32) b_sz = BLOCK_SIZE;
    else if(N == 64) b_sz = BLOCK_SIZE/2;
    else b_sz = BLOCK_SIZE*2;

    if(N == M && M == 64) trans_64(A,B);
    else{
        for (j = 0; j < M; j+=b_sz) {
            for (i = 0; i < N; i += b_sz) {
                for(i1 = i; (i1 < i+b_sz) && (i1 < N); i1++){
                    for(j1 = j; (j1 < j+b_sz) && (j1 < M); j1++){
                        if(i1 != j1) B[j1][i1] = A[i1][j1];
                        else{
                            tmp = A[i1][j1];
                            diag_idx = i1;
                        }
                    }
                    /* if row_block == col_block, 
                    it can gaurantee that there is one in a diagonal every row*/
                    if(i == j) B[diag_idx][diag_idx] = tmp;
                }
            }
        } 
    }  
    ENSURES(is_transpose(M, N, A, B));
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 
void trans_64(int A[64][64], int B[64][64]){
/*transpose two 4*8 sub-blocks in the order of:
from
0000ffff
1111eeee
2222dddd
3333cccc
4444bbbb
5555aaaa
66669999
77778888
to
01234567
01234567
01234567
01234567
fedcba98
fedcba98
fedcba98
fedcba98
*/
int i,j,k,a0,a1,a2,a3,a4,a5,a6,a7;
int *ptr;
    for(i = 0; i < 64; i+=BLOCK_SIZE){
        for(j = 0; j < 64; j+=BLOCK_SIZE){
            /*0~7*/
            for(k = 0; k < BLOCK_SIZE; k++){
                ptr = &A[j+k][i];
                a0 = ptr[0];
                a1 = ptr[1];
                a2 = ptr[2];
                a3 = ptr[3];
                /*saving 'f's*/
                if(!k){
                    a4 = ptr[4];
                    a5 = ptr[5];
                    a6 = ptr[6];
                    a7 = ptr[7];
                }
                ptr = &B[i][j+k];
                ptr[0] = a0;
                ptr[64] = a1;
                ptr[128] = a2;
                ptr[192] = a3;
            }
            /*8 to e*/
            for(k = 7; k > 0; k--){
                ptr = &A[j+k][i+4];
                a0 = ptr[0];
                a1 = ptr[1];
                a2 = ptr[2];
                a3 = ptr[3];
                ptr = &B[i+4][j+k];
                ptr[0] = a0;
                ptr[64] = a1;
                ptr[128] = a2;
                ptr[192] = a3;
            }
            /* f */
            ptr = &B[i+4][j];
            ptr[0] = a4;
            ptr[64] = a5;
            ptr[128] = a6;
            ptr[192] = a7;
        }
    }

}
/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    //registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


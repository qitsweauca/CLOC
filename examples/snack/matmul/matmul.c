/*
 
   matmul.c : Matrix Multiplication Example - Host Code
 
   This example shows the benefit of using tiled Kernels with local data.
   It calls three different kernels, simple_sgemm_tt, tiled_sgemm_tt, and tiled_sgemm_tn.
   See matmulKernels.cl for the implementations of these kernels in c. 
   In most cases, tiled_sgemm_tt will run much faster than simple_stemm_tt.
   
   For comparison, A  CPU version of sgemm_tn is included in this file. 
 
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/param.h> 
#include <time.h>
#include "matmul.h"

typedef struct {
   int width;
   int height;
   int stride;
   int hpad;
   float* elements; 
} Matrix;

u_int32_t arc4random();
long int get_nanosecs( struct timespec start_time, struct timespec end_time) ;
void CPU_sgemm_tn(const int M,const int N,const int K,const float alpha, const float* A,const int LDA, 
     const float* B,const int LDB, const float beta,float* C, const int LDC) ;

/*  The following header file is generated by cloc -c matmulKernels.cl */
#include "matmulKernels.h"

int main(int argc, char* argv[]){

   Matrix A,B,Bt,C;
   int a1,a2,a3,i,j;
   struct timespec start_time, end_time;
   long int nanosecs,total_ops;
   float gflopsSimple,gflopsTiled,gflopsCPU,gflopsTiledtn;

   /*  Read dimensions, allocate storage and generate random matrix */
   a1 = atoi(argv[1]); /* Height of A */
   a2 = atoi(argv[2]); /* Width of A , and Height of B*/
   a3 = atoi(argv[3]); /* Width of B */
   total_ops = 2 * (long int) a1 * (long int) a2 * (long int) a3;
   printf("\n\n");

   A.height = a1;
   A.width = a2;
   A.stride = (((A.width-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   A.hpad = (((A.height-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   A.elements = (float*)malloc_global(A.stride * A.hpad* sizeof(float));

   B.height = a2;
   B.width = a3;
   B.stride = (((B.width-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   B.hpad = (((B.height-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   B.elements = (float*)malloc_global(B.stride * B.hpad * sizeof(float));

   /* Bt is same as B but stored in column-major order */
   Bt.height = B.height; 
   Bt.width = B.width;
   Bt.stride = B.stride;
   Bt.hpad = B.hpad;
   Bt.elements = (float*)malloc_global(Bt.stride * Bt.hpad * sizeof(float));

   C.height = a1;
   C.width = a3;
   C.stride = (((C.width-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   C.hpad = (((C.height-1)/BLOCK_SIZE)+1) * BLOCK_SIZE;
   C.elements = (float*)malloc_global(C.stride * C.hpad * sizeof(float));

   for(i = 0; i < A.hpad ; i++)
      for(j = 0; j < A.stride; j++) {
         if (( j<A.width ) && (i<A.height)) {
            A.elements[i*A.stride + j] = (arc4random() % 3);   
         } else {
            A.elements[i*A.stride + j] = 0.0;
         }
      }

   for(i = 0; i < B.hpad ; i++)
      for(j = 0; j < B.stride; j++) {
         if (( j<B.width ) && (i<B.height)) {
            B.elements[i*B.stride+j] = (arc4random() % 2);   
            Bt.elements[j*Bt.stride+i] = B.elements[i*B.stride+j] ;
         } else {
            B.elements[i*B.stride+j] = 0.0;
            Bt.elements[j*Bt.stride+i] = 0.0;
         }
      }

   /* zero C */
   for(i = 0; i < C.hpad; i++)
      for(j = 0; j < C.stride; j++)
         C.elements[i*C.stride+j] = 0.0;

   printf("Matrix A: %d by %d \n",A.height,A.width);
   for(i = 0; i < MIN(10, A.height); i++){
      for(j = 0; j < MIN(10, A.width); j++)
         printf("%.0f ", A.elements[i*A.stride+j]);
      printf("\n");
   }
   printf("\n");

   printf("Matrix B: %d by %d \n",B.height,B.width);
   for(i = 0; i < MIN(10, B.height); i++){
      for(j = 0; j < MIN(10, B.width); j++)
         printf("%.0f ", B.elements[i*B.stride+j]);
      printf("\n");
   }
   printf("\n");
/*   
   printf("Matrix Bt: %d by %d \n",Bt.height,Bt.width);
   for(i = 0; i < MIN(10, Bt.height); i++){
      for(j = 0; j < MIN(10, Bt.width); j++)
         printf("%.0f ", Bt.elements[j*Bt.stride+i]);
      printf("\n");
   }
   printf("\n");
*/
   printf("Calling sgemm_tn on CPU... \n");
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);
   CPU_sgemm_tn(A.height,Bt.width,Bt.height,1.0,A.elements,A.stride,Bt.elements,Bt.stride,1.0,C.elements,C.stride); 
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);
   nanosecs = get_nanosecs(start_time,end_time);
   gflopsCPU= (float) total_ops / (float) nanosecs ;

   printf("Matrix C: %d by %d  after CPU sgemm_tt \n",C.height,C.width);
   for(i = 0; i < MIN(10, C.height); i++){
      for(j = 0; j < MIN(10, C.width); j++)
         printf("%6.0f ", C.elements[i*C.stride + j]);
      printf("\n");
   }
   printf("\n");

   /* zero C */
   for(i = 0; i < C.hpad; i++)
      for(j = 0; j < C.stride; j++)
         C.elements[i*C.stride+j] = 0.0;

   printf("Calling Simple Kernel ... \n");
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);
   SNK_INIT_LPARM(lparm,0);
   lparm->ndim=2;
   lparm->gdims[0]=C.hpad;
   lparm->gdims[1]=C.stride;
   lparm->ldims[0]=BLOCK_SIZE;
   lparm->ldims[1]=BLOCK_SIZE;
   simple_sgemm_tt(A.height,B.width,B.height,1.0,A.elements,A.stride,B.elements,B.stride,1.0,C.elements,C.stride,lparm); 
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);
   nanosecs = get_nanosecs(start_time,end_time);
   gflopsSimple = (float) total_ops / (float) nanosecs ;

   printf("Matrix C: %d by %d  after simple_sgemm_tt kernel\n",C.height,C.width);
   for(i = 0; i < MIN(10, C.height); i++){
      for(j = 0; j < MIN(10, C.width); j++)
         printf("%6.0f ", C.elements[i*C.stride+j]);
      printf("\n");
   }
   printf("\n");

   /* zero C */
   for(i = 0; i < C.hpad; i++)
      for(j = 0; j < C.stride; j++)
         C.elements[i*C.stride+j] = 0.0;

   printf("Calling Tiled Kernel tt ... \n");
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);
   tiled_sgemm_tt(A.height,B.width,B.height,1.0,A.elements,A.stride,B.elements,B.stride,1.0,C.elements,C.stride,lparm); 
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);
   nanosecs = get_nanosecs(start_time,end_time);
   gflopsTiled = (float) total_ops / (float) nanosecs ;

   printf("Matrix C: %d by %d  after tiled_sgemm_tt kernel\n",C.height,C.width);
   for(i = 0; i < MIN(10, C.height); i++){
      for(j = 0; j < MIN(10, C.width); j++)
         printf("%6.0f ", C.elements[i*C.stride+j]);
      printf("\n");
   }
   printf("\n");

   /* zero C */
   for(i = 0; i < C.hpad; i++)
      for(j = 0; j < C.stride; j++)
         C.elements[i*C.stride+j] = 0.0;

   printf("Calling Tiled Kernel tn ... \n");
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&start_time);
   tiled_sgemm_tn(A.height,Bt.width,Bt.height,1.0,A.elements,A.stride,Bt.elements,Bt.stride,1.0,C.elements,C.stride,lparm); 
   clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&end_time);
   nanosecs = get_nanosecs(start_time,end_time);
   gflopsTiledtn = (float) total_ops / (float) nanosecs ;

   printf("Matrix C: %d by %d  after tiled_sgemm_tn kernel\n",C.height,C.width);
   for(i = 0; i < MIN(10, C.height); i++){
      for(j = 0; j < MIN(10, C.width); j++)
         printf("%6.0f ", C.elements[i*C.stride+j]);
      printf("\n");
   }
   printf("\n");

   printf("Dimensions=%d %d %d\n",a1,a2,a3);
   printf("GFLOPS for CPU             =  %6.4f\n",gflopsCPU);
   printf("GFLOPS for Simple Kernel   =  %6.2f\n",gflopsSimple);
   printf("GFLOPS for Tiled Kernel tt =  %6.2f\n",gflopsTiled);
   printf("GFLOPS for Tiled Kernel tn =  %6.2f\n",gflopsTiledtn);

}

#define NSECPERSEC 1000000000L
/* Correctly extract the number of nanoseconds from the two time structures */
long int get_nanosecs( struct timespec start_time, struct timespec end_time) {
   long int nanosecs;
   if ((end_time.tv_nsec-start_time.tv_nsec)<0) nanosecs = 
      ((((long int) end_time.tv_sec- (long int) start_time.tv_sec )-1)*NSECPERSEC ) +
      ( NSECPERSEC + (long int) end_time.tv_nsec - (long int) start_time.tv_nsec) ;
   else nanosecs = 
      (((long int) end_time.tv_sec- (long int) start_time.tv_sec )*NSECPERSEC ) +
      ( (long int) end_time.tv_nsec - (long int) start_time.tv_nsec );
   return nanosecs;
}

void CPU_sgemm_tn(const int M,const int N,const int K,const float alpha, const float* A,const int LDA, 
const float* B,const int LDB, const float beta,float* C, const int LDC) {
   /*  A and C  are in row-major order */
   /*  B is in column-major order */
   int c_row,c_col,inner;
   float sum;
   for (c_col  = 0 ;  c_col<N; c_col++ ) {
      for (c_row = 0 ; c_row<M; c_row++ ) {
         sum = 0.0 ;
         for (inner = 0 ; inner<K; inner++ ) {
            sum += A[c_row*LDA + inner] * B[c_col*LDB + inner] ;
         }
         C[c_row*LDC + c_col] = alpha*sum + beta*C[ c_row*LDC + c_col] ;
      }
   }
}

#ifndef TASK_H
#define TASK_H

#define TASK_FCONV2D 0
#define TASK_FMATMUL 1
#define TASK_BOTH    2
#define TASK_CHECK   3

// Layout in untrusted memory (UTM):
//   offset 0: struct task_info
//   offset 64: fconv2d output buffer (112*112*8 = 100352 bytes)
//   offset 100416: fmatmul output buffer (128*128*8 = 131072 bytes)
//   offset 231488: fconv2d scalar output (for reference)
//   offset 331840: fmatmul scalar output

typedef struct {
    int task_type;      // TASK_FCONV2D, TASK_FMATMUL, TASK_BOTH, TASK_CHECK
    int num_chunks;     // total number of chunks for this task type
    int chunk_id;       // which chunk this is (0..num_chunks-1)
    int start_row;      // computed row range start
    int end_row;        // computed row range end
    int64_t runtime_vec;  // timing: vector computation
    int64_t runtime_scalar; // timing: scalar computation
    int pass;           // 1 if verification passed
    int total_rows;     // total rows for the full matrix
    int total_cols;     // total columns
} task_info_t;

#define UTM_OFFSET_TASK      0
#define UTM_OFFSET_FC_OUT    64
#define UTM_OFFSET_FC_REF    (64 + 112*112*8)
#define UTM_OFFSET_MM_OUT    (64 + 112*112*8 + 112*112*8)
#define UTM_OFFSET_MM_REF    (64 + 112*112*8 + 112*112*8 + 128*128*8)

#define FCONV2D_M 112
#define FCONV2D_N 112
#define FMATMUL_M 128

#endif

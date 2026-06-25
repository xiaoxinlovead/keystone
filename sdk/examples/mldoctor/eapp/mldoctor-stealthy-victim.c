#include "mldoctor-result.h"

#include <inttypes.h>
#include <stdint.h>
#include <string.h>
#include "printf.h"

#define CLASS_COUNT 5u
#define MAX_PER_CLASS 64u
#define VECTOR_SIZE 128u
#define IMAGE_SIZE 16u

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))


static float input_data[IMAGE_SIZE * IMAGE_SIZE] __attribute__((aligned(64)));
static float feature_data[IMAGE_SIZE * IMAGE_SIZE] __attribute__((aligned(64)));
static float depthwise_data[IMAGE_SIZE * IMAGE_SIZE] __attribute__((aligned(64)));
static float dense_weights[CLASS_COUNT * VECTOR_SIZE] __attribute__((aligned(64)));
static float dense_output[CLASS_COUNT] __attribute__((aligned(64)));
static uint32_t split_labels[CLASS_COUNT * MAX_PER_CLASS] __attribute__((aligned(64)));
static volatile float result_sink;
static volatile uint64_t marker_sink;

static inline uint64_t read_time(void) {
  uint64_t value;
  __asm__ volatile("csrr %0, time" : "=r"(value));
  return value;
}

static uint32_t next_random(uint32_t *state) {
  uint32_t value = *state;
  value ^= value << 13;
  value ^= value >> 17;
  value ^= value << 5;
  *state = value;
  return value;
}

__attribute__((noinline)) static void sample_begin_marker(uint32_t label,
                                                          uint32_t sequence) {
  marker_sink ^= 0xb367000000000000ull | ((uint64_t)label << 32) | sequence;
}

__attribute__((noinline)) static void sample_end_marker(uint32_t label,
                                                        uint32_t sequence) {
  marker_sink ^= 0xe0d0000000000000ull | ((uint64_t)label << 32) | sequence;
}

__attribute__((noinline)) static void launch_conv(uint32_t repeat,
                                                  uint32_t salt) {
  for (uint32_t r = 0; r < repeat; r++) {
    for (uint32_t y = 1; y + 1 < IMAGE_SIZE; y++) {
      for (uint32_t x = 1; x + 1 < IMAGE_SIZE; x++) {
        float sum = 0.0f;
        for (uint32_t ky = 0; ky < 3; ky++) {
          for (uint32_t kx = 0; kx < 3; kx++) {
            uint32_t index = (y + ky - 1) * IMAGE_SIZE + x + kx - 1;
            float weight = (float)(((kx + ky + salt) % 5u) + 1u) * 0.03125f;
            sum += input_data[index] * weight;
          }
        }
        feature_data[y * IMAGE_SIZE + x] += sum;
      }
    }
  }
}

__attribute__((noinline)) static void depthwise_conv2d(uint32_t repeat,
                                                       uint32_t salt) {
  for (uint32_t r = 0; r < repeat; r++) {
    for (uint32_t y = 1; y + 1 < IMAGE_SIZE; y++) {
      for (uint32_t x = 1; x + 1 < IMAGE_SIZE; x++) {
        uint32_t center = y * IMAGE_SIZE + x;
        float edge = feature_data[center - 1] + feature_data[center + 1] +
                     feature_data[center - IMAGE_SIZE] +
                     feature_data[center + IMAGE_SIZE] -
                     4.0f * feature_data[center];
        depthwise_data[center] +=
            edge * (float)((salt % 7u) + 1u) * 0.015625f;
      }
    }
  }
}

__attribute__((noinline)) static void relu6(uint32_t repeat) {
  for (uint32_t r = 0; r < repeat; r++) {
    for (uint32_t i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
      float value = depthwise_data[i];
      if (value < 0.0f) value = 0.0f;
      if (value > 6.0f) value = 6.0f;
      depthwise_data[i] = value;
    }
  }
}

__attribute__((noinline)) static void average_pool(uint32_t repeat) {
  float sum = 0.0f;
  for (uint32_t r = 0; r < repeat; r++) {
    for (uint32_t i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
      sum += depthwise_data[i];
    }
    sum *= 1.0f / (float)(IMAGE_SIZE * IMAGE_SIZE);
  }
  result_sink += sum;
}

__attribute__((noinline)) static void matmul(uint32_t repeat, uint32_t salt) {
  for (uint32_t r = 0; r < repeat; r++) {
    for (uint32_t output = 0; output < CLASS_COUNT; output++) {
      float sum = 0.0f;
      for (uint32_t i = 0; i < VECTOR_SIZE; i++) {
        sum += depthwise_data[(i + salt) % (IMAGE_SIZE * IMAGE_SIZE)] *
               dense_weights[output * VECTOR_SIZE + i];
      }
      dense_output[output] += sum;
    }
  }
}

static void init_sample(uint32_t label, uint32_t ordinal) {
  uint32_t state = 0x9e3779b9u ^ (label * 0x45d9f3bu) ^ ordinal;
  for (uint32_t i = 0; i < IMAGE_SIZE * IMAGE_SIZE; i++) {
    input_data[i] = (float)(next_random(&state) & 0xffu) * (1.0f / 255.0f);
    feature_data[i] = 0.0f;
    depthwise_data[i] = 0.0f;
  }
  for (uint32_t i = 0; i < CLASS_COUNT * VECTOR_SIZE; i++) {
    dense_weights[i] =
        (float)((int32_t)(next_random(&state) % 31u) - 15) * 0.0078125f;
  }
  memset(dense_output, 0, sizeof(dense_output));
}

static void run_sample(uint32_t label, uint32_t ordinal) {
  static const uint8_t conv_repeat[CLASS_COUNT] = {2, 3, 4, 2, 5};
  static const uint8_t depthwise_repeat[CLASS_COUNT] = {3, 2, 4, 5, 3};
  static const uint8_t relu_repeat[CLASS_COUNT] = {2, 4, 3, 5, 2};
  static const uint8_t pool_repeat[CLASS_COUNT] = {1, 2, 1, 3, 2};
  static const uint8_t matmul_repeat[CLASS_COUNT] = {2, 3, 5, 4, 2};

  init_sample(label, ordinal);
  launch_conv(conv_repeat[label], ordinal + label);
  depthwise_conv2d(depthwise_repeat[label], ordinal + 3u * label);
  relu6(relu_repeat[label]);
  average_pool(pool_repeat[label]);
  matmul(matmul_repeat[label], ordinal + label);
  result_sink += dense_output[label];
}

static void shuffle(uint32_t *values, uint32_t count, uint32_t seed) {
  uint32_t state = seed ? seed : 1u;
  for (uint32_t index = count; index > 1; index--) {
    uint32_t other = next_random(&state) % index;
    uint32_t tmp = values[index - 1u];
    values[index - 1u] = values[other];
    values[other] = tmp;
  }
}

static uint32_t prepare_labels(uint32_t per_class, uint32_t seed) {
  uint32_t count = CLASS_COUNT * per_class;
  for (uint32_t label = 0; label < CLASS_COUNT; label++) {
    for (uint32_t index = 0; index < per_class; index++) {
      split_labels[label * per_class + index] = label;
    }
  }
  shuffle(split_labels, count, seed);
  return count;
}

static void run_label_window(const char *split, uint32_t sequence,
                             uint32_t label, uint32_t emit_sample_log) {
  sample_begin_marker(label, sequence);
  if (emit_sample_log) {
    uint64_t start = read_time();
    run_sample(label, sequence);
    uint64_t end = read_time();
    sample_end_marker(label, sequence);
    printf("[STEALTHY_SAMPLE] sample_id=%s-%05u label=%u split=%s "
           "start_cycle=%" PRIu64 " end_cycle=%" PRIu64 "\n",
           split, sequence, label,
           split, start, end);
  } else {
    run_sample(label, sequence);
    sample_end_marker(label, sequence);
  }
}

static void run_split(const char *split, uint32_t per_class, uint32_t seed,
                      uint32_t emit_sample_log) {
  uint32_t count = prepare_labels(per_class, seed);
  for (uint32_t sequence = 0; sequence < count; sequence++) {
    run_label_window(split, sequence, split_labels[sequence], emit_sample_log);
  }
}

int run_mldoctor(void) {
  uint32_t attack_train_per_class = 1;
  uint32_t test_per_class = 1;
  uint32_t seed = 7;
  uint32_t emit_sample_log = 1;

  printf("[STEALTHY_CONFIG] attack_train_per_class=%u test_per_class=%u "
         "seed=%u class_count=%u sample_log=%u"
         " evidence=synthetic_riscv_function_victim(enclave)\n",
         attack_train_per_class, test_per_class, seed, CLASS_COUNT,
         emit_sample_log);
  printf("[STEALTHY_READY] time=%" PRIu64 "\n", read_time());

  run_split("attack_train", attack_train_per_class, seed ^ 0x13579bdu,
            emit_sample_log);
  run_split("test", test_per_class, seed ^ 0x2468aceu, emit_sample_log);

  uint32_t sample_count =
      CLASS_COUNT * (attack_train_per_class + test_per_class);
  mld_result_begin("stealthy-dnn-label-inference-xiangshan-victim",
                   "label_inference_cache",
                   "RISC-V enclave synthetic victim",
                   "done");
  mld_result_kv_str("metric_name", "stealthy_victim_sample_count");
  mld_result_kv_u64("metric_value", sample_count);
  mld_result_kv_str("higher_is_better", "diagnostic");
  mld_result_kv_str("normalized_score", "");
  mld_result_kv_u64("attack_train_per_class", attack_train_per_class);
  mld_result_kv_u64("test_per_class", test_per_class);
  mld_result_kv_u64("checksum", (uint64_t)(result_sink * 1000000.0f));
  mld_result_kv_u64("marker_checksum", marker_sink);
  mld_result_kv_str(
      "residual_gap",
      "synthetic MobileNet-style function victim, adapted for Keystone enclave");
  mld_result_end();
  return 0;
}

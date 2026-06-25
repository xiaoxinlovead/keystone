#ifndef MLDOCTOR_RESULT_H
#define MLDOCTOR_RESULT_H

#include <inttypes.h>
#include <stdint.h>
#include "printf.h"

static inline void mld_result_begin(const char *attack_id, const char *family,
                                    const char *evidence_level, const char *status) {
  printf("[MLD_RESULT_BEGIN]\n");
  printf("attack_id=%s\n", attack_id);
  printf("family=%s\n", family);
  printf("evidence_level=%s\n", evidence_level);
  printf("status=%s\n", status);
}

static inline void mld_result_kv_u64(const char *key, uint64_t value) {
  printf("%s=%" PRIu64 "\n", key, value);
}

static inline void mld_result_kv_str(const char *key, const char *value) {
  printf("%s=%s\n", key, value);
}

static inline void mld_result_metric_double(const char *name, double value,
                                            int higher_is_better, double normalized_score) {
  printf("metric_name=%s\n", name);
  printf("metric_value=%.6f\n", value);
  printf("higher_is_better=%d\n", higher_is_better);
  printf("normalized_score=%.6f\n", normalized_score);
}

static inline void mld_result_end(void) {
  printf("[MLD_RESULT_END]\n");
}

#endif

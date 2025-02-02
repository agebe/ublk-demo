#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <linux/types.h>
#include "utils.h"

// Function to convert string to unsigned long long (to handle larger numbers)
__u64 stringToByteSize(const char *input) {
  __u64 result = 0;
  char unit[10];
  double value = 0;
  int i;
  // Parse the number part of the string
  for (i = 0; input[i] != '\0' && isdigit(input[i]); i++) {
    value = value * 10 + (input[i] - '0');
  }
  // Skip any spaces or decimal points (for simplicity, we'll ignore fractional parts)
  while (isspace(input[i]) || input[i] == '.') i++;
  // Parse the unit
  int u = 0;
  while (input[i] && u < 9) {
    unit[u++] = tolower(input[i++]);
  }
  unit[u] = '\0';
  // Convert based on the unit
  if (strcmp(unit, "b") == 0 || strcmp(unit, "byte") == 0) {
    result = (__u64)value;
  } else if (strcmp(unit, "kb") == 0 || strcmp(unit, "k") == 0 || strcmp(unit, "kilobyte") == 0) {
    result = (__u64)(value * 1024);
  } else if (strcmp(unit, "mb") == 0 || strcmp(unit, "m") == 0 || strcmp(unit, "megabyte") == 0) {
    result = (__u64)(value * 1024 * 1024);
  } else if (strcmp(unit, "gb") == 0 || strcmp(unit, "g") == 0 || strcmp(unit, "gigabyte") == 0) {
    result = (__u64)(value * 1024 * 1024 * 1024);
  } else if (strcmp(unit, "tb") == 0 || strcmp(unit, "t") == 0 || strcmp(unit, "terabyte") == 0) {
    result = (__u64)(value * 1024 * 1024 * 1024 * 1024);
  } else {
    fprintf(stderr, "Unknown unit: %s\n", unit);
    return 0; // or some error value
  }
  return result;
}

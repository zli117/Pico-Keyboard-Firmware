#include <stdio.h>

#include "FreeRTOS.h"
#include "pico/stdlib.h"
#include "runner.h"
#include "storage.h"
#include "sync.h"
#include "utils.h"

extern "C" void vApplicationMallocFailedHook(void) {
  LOG_ERROR("Failed malloc. OOM");
}
extern "C" void vApplicationIdleHook(void) {}
extern "C" void vApplicationStackOverflowHook(TaskHandle_t pxTask,
                                              char *pcTaskName) {
  LOG_ERROR("Stack overflow for task %s", pcTaskName);
}
extern "C" void vApplicationTickHook(void) {}

int main() {
  if (InitializeStorage() == OK &&   //
      runner::RunnerInit() == OK &&  //
      runner::RunnerStart() == OK) {
    vTaskStartScheduler();
  }

  while (true)
    ;

  return 0;
}

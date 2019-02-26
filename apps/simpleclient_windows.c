/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

#include "oc_api.h"
#include "oc_obt.h"
#include "port/oc_clock.h"
#include "sig.h"
#include <signal.h>
#include <windows.h>

int quit = 0;

static CONDITION_VARIABLE cv;
static CRITICAL_SECTION cs;

static void signal_event_loop(void)
{
  WakeConditionVariable(&cv);
}

void handle_signal(int signal)
{
  signal_event_loop();
  quit = 1;
}

int main(void)
{
  InitializeCriticalSection(&cs);
  InitializeConditionVariable(&cv);

  int init;

  signal(SIGINT, handle_signal);

  static const oc_handler_t handler = {.init = sig_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources = sig_register,
                                       .requests_entry = sig_discovery };

  oc_clock_time_t next_event;

#ifdef OC_SECURITY
  //oc_storage_config("./simpleclient_creds/");
#endif /* OC_SECURITY */

  init = oc_main_init(&handler);
  if (init < 0)
    return init;

  while (quit != 1) {
    next_event = oc_main_poll();
    if (next_event == 0) {
      SleepConditionVariableCS(&cv, &cs, INFINITE);
    } else {
      oc_clock_time_t now = oc_clock_time();
      if (now < next_event) {
          SleepConditionVariableCS(&cv, &cs,
              (DWORD)((next_event - now) * 1000 / OC_CLOCK_SECOND));
      }
    }
  }

  oc_main_shutdown();
  return 0;
}

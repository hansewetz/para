// (C) Copyright Hans Ewetz 2019. All rights reserved.
#pragma once

// level to log message at
enum errlevel_t{DEBUG=0,INFO=1,WARNING=2,ERROR=3,FATAL=4};

void loglevel(enum errlevel_t cutoff);                          // set cutoff error level
void app_message(enum errlevel_t level, char const*msg,...);    // print an application message and continue

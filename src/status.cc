// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "status.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

StatusPrinter::StatusPrinter(const BuildConfig& config)
    : config_(config),
      started_edges_(0), finished_edges_(0), total_edges_(0), running_edges_(0),
      time_millis_(0), progress_status_format_(NULL),
      current_rate_(config.parallelism) {

  // Don't do anything fancy in verbose mode.
  if (config_.verbosity != BuildConfig::NORMAL)
    printer_.set_smart_terminal(false);

  progress_status_format_ = getenv("NINJA_STATUS");
  if (!progress_status_format_)
    progress_status_format_ = "[%f/%t] ";
}

void StatusPrinter::PlanHasTotalEdges(int total) {
  total_edges_ = total;
}

void StatusPrinter::BuildEdgeStarted(Edge* edge, int64_t start_time_millis) {
  ++started_edges_;
  ++running_edges_;
  time_millis_ = start_time_millis;

  if (edge->use_console() || printer_.is_smart_terminal())
    PrintStatus(edge, start_time_millis);

  if (edge->use_console())
    printer_.SetConsoleLocked(true);
}

void StatusPrinter::BuildEdgeFinished(Edge* edge, int64_t end_time_millis,
                                      const CommandRunner::Result* result) {
  time_millis_ = end_time_millis;
  ++finished_edges_;

  if (edge->use_console())
    printer_.SetConsoleLocked(false);

  if (config_.verbosity == BuildConfig::QUIET)
    return;

  if (!edge->use_console())
    PrintStatus(edge, end_time_millis);

  --running_edges_;

  // Print the command that is spewing before printing its output.
  if (!result->success()) {
    string outputs;
    for (vector<Node*>::const_iterator o = edge->outputs_.begin();
         o != edge->outputs_.end(); ++o)
      outputs += (*o)->path() + " ";

    printer_.PrintOnNewLine("FAILED: " + outputs + "\n");
    printer_.PrintOnNewLine(edge->EvaluateCommand() + "\n");
  }

  if (!result->output.empty()) {
    // ninja sets stdout and stderr of subprocesses to a pipe, to be able to
    // check if the output is empty. Some compilers, e.g. clang, check
    // isatty(stderr) to decide if they should print colored output.
    // To make it possible to use colored output with ninja, subprocesses should
    // be run with a flag that forces them to always print color escape codes.
    // To make sure these escape codes don't show up in a file if ninja's output
    // is piped to a file, ninja strips ansi escape codes again if it's not
    // writing to a |smart_terminal_|.
    // (Launching subprocesses in pseudo ttys doesn't work because there are
    // only a few hundred available on some systems, and ninja can launch
    // thousands of parallel compile commands.)
    // TODO: There should be a flag to disable escape code stripping.
    string final_output;
    if (!printer_.is_smart_terminal())
      final_output = StripAnsiEscapeCodes(result->output);
    else
      final_output = result->output;
    printer_.PrintOnNewLine(final_output);
  }
}

void StatusPrinter::BuildStarted() {
  started_edges_ = 0;
  finished_edges_ = 0;
  running_edges_ = 0;
}

void StatusPrinter::BuildFinished() {
  printer_.SetConsoleLocked(false);
  printer_.PrintOnNewLine("");
}

string StatusPrinter::FormatProgressStatus(const char* progress_status_format,
                                           int64_t time_millis) const {
  string out;
  char buf[32];
  for (const char* s = progress_status_format; *s != '\0'; ++s) {
    if (*s == '%') {
      ++s;
      switch (*s) {
      case '%':
        out.push_back('%');
        break;

        // Started edges.
      case 's':
        snprintf(buf, sizeof(buf), "%d", started_edges_);
        out += buf;
        break;

        // Total edges.
      case 't':
        snprintf(buf, sizeof(buf), "%d", total_edges_);
        out += buf;
        break;

        // Running edges.
      case 'r': {
        snprintf(buf, sizeof(buf), "%d", running_edges_);
        out += buf;
        break;
      }

        // Unstarted edges.
      case 'u':
        snprintf(buf, sizeof(buf), "%d", total_edges_ - started_edges_);
        out += buf;
        break;

        // Finished edges.
      case 'f':
        snprintf(buf, sizeof(buf), "%d", finished_edges_);
        out += buf;
        break;

        // Overall finished edges per second.
      case 'o':
        SnprintfRate(finished_edges_ / (time_millis_ / 1e3), buf, "%.1f");
        out += buf;
        break;

        // Current rate, average over the last '-j' jobs.
      case 'c':
        current_rate_.UpdateRate(finished_edges_, time_millis_);
        SnprintfRate(current_rate_.rate(), buf, "%.1f");
        out += buf;
        break;

        // Percentage
      case 'p': {
        int percent = (100 * finished_edges_) / total_edges_;
        snprintf(buf, sizeof(buf), "%3i%%", percent);
        out += buf;
        break;
      }

      case 'e': {
        snprintf(buf, sizeof(buf), "%.3f", time_millis_ / 1e3);
        out += buf;
        break;
      }

      default:
        Fatal("unknown placeholder '%%%c' in $NINJA_STATUS", *s);
        return "";
      }
    } else {
      out.push_back(*s);
    }
  }

  return out;
}

void StatusPrinter::PrintStatus(Edge* edge, int64_t time_millis) {
  if (config_.verbosity == BuildConfig::QUIET)
    return;

  bool force_full_command = config_.verbosity == BuildConfig::VERBOSE;

  string to_print = edge->GetBinding("description");
  if (to_print.empty() || force_full_command)
    to_print = edge->GetBinding("command");

  to_print = FormatProgressStatus(progress_status_format_, time_millis)
      + to_print;

  printer_.Print(to_print,
                 force_full_command ? LinePrinter::FULL : LinePrinter::ELIDE);
}

void StatusPrinter::Warning(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  ::Warning(msg, ap);
  va_end(ap);
}

void StatusPrinter::Error(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  ::Error(msg, ap);
  va_end(ap);
}

void StatusPrinter::Info(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  ::Info(msg, ap);
  va_end(ap);
}

#ifndef _WIN32

StatusSerializer::StatusSerializer(const BuildConfig& config) :
    config_(config), serializer_(NULL), subprocess_(NULL) {
  int output_pipe[2];
  if (pipe(output_pipe) < 0)
    Fatal("pipe: %s", strerror(errno));
  SetCloseOnExec(output_pipe[1]);

  serializer_ = new Serializer(output_pipe[1]);

  subprocess_ = subprocess_set_.Add(config.frontend, /*use_console=*/true,
                                    output_pipe[0]);
  close(output_pipe[0]);

  serializer_->Uint(kHeader);
}

StatusSerializer::~StatusSerializer() {
  serializer_->Flush();
  delete serializer_;
  subprocess_->Finish();
  subprocess_set_.Clear();
}

void StatusSerializer::PlanHasTotalEdges(int total) {
  serializer_->Array(2);
  serializer_->Uint(kTotalEdges);
  serializer_->Uint(total);
  serializer_->Flush();
}

void StatusSerializer::BuildEdgeStarted(Edge* edge, int64_t start_time_millis) {
  serializer_->Array(8);
  serializer_->Uint(kEdgeStarted);
  serializer_->Uint(edge->id_);
  serializer_->Uint(start_time_millis);
  serializer_->Array(edge->inputs_.size());
  for (vector<Node*>::iterator it = edge->inputs_.begin(); it != edge->inputs_.end(); ++it) {
    serializer_->String((*it)->path());
  }
  serializer_->Array(edge->outputs_.size());
  for (vector<Node*>::iterator it = edge->outputs_.begin(); it != edge->outputs_.end(); ++it) {
    serializer_->String((*it)->path());
  }
  serializer_->String(edge->GetBinding("description"));
  serializer_->String(edge->GetBinding("command"));
  serializer_->Bool(edge->use_console());
  serializer_->Flush();
}

void StatusSerializer::BuildEdgeFinished(Edge* edge, int64_t end_time_millis,
                                         const CommandRunner::Result* result) {
  serializer_->Array(5);
  serializer_->Uint(kEdgeFinished);
  serializer_->Uint(edge->id_);
  serializer_->Uint(end_time_millis);
  serializer_->Int(result->status);
  serializer_->String(result->output);
  serializer_->Flush();
}

void StatusSerializer::BuildStarted() {
  serializer_->Array(3);
  serializer_->Uint(kBuildStarted);
  serializer_->Uint(config_.parallelism);
  serializer_->Bool(config_.verbosity == BuildConfig::VERBOSE);
}

void StatusSerializer::BuildFinished() {
  serializer_->Array(1);
  serializer_->Uint(kBuildFinished);
}

void StatusSerializer::Message(messageType type, const char* msg,
                    va_list ap) {
  va_list ap2;
  va_copy(ap2, ap);

  int len = vsnprintf(NULL, 0, msg, ap2);
  if (len < 0) {
    Fatal("vsnprintf failed");
  }

  va_end(ap2);

  char* buf = new char[len + 1];
  buf[0] = 0;
  vsnprintf(buf, len + 1, msg, ap);
  buf[len] = 0;

  serializer_->Array(2);
  serializer_->Uint(type);
  serializer_->String(buf);
  serializer_->Flush();
}

void StatusSerializer::Info(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Message(kNinjaInfo, msg, ap);
  va_end(ap);
}

void StatusSerializer::Warning(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Message(kNinjaWarning, msg, ap);
  va_end(ap);
}

void StatusSerializer::Error(const char* msg, ...) {
  va_list ap;
  va_start(ap, msg);
  Message(kNinjaError, msg, ap);
  va_end(ap);
}
#endif // !_WIN32

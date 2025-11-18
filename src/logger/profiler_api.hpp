#pragma once

#if PROF_ENABLED

  #include "profiler.hpp"

// Resolve a string literal to an event ID once per call site.
  #define PROF_REGISTER_EVENT(name_literal) \
      ([]() -> std::uint16_t { \
          static const std::uint16_t __prof_id = \
              ::Profiler::instance().register_event_literal(name_literal, sizeof(name_literal) - 1); \
          return __prof_id; \
      }())

  #define PROF_THREAD_INIT()    ::Profiler::instance().thread_init()
  #define PROF_THREAD_FLUSH()   ::Profiler::instance().thread_merge_data()
  #define PROF_START(id)  ::Profiler::instance().start(id)
  #define PROF_STOP(id)   ::Profiler::instance().stop (id)
  #define PROF_EVENT(message)    ::Profiler::instance().log_event(message)
  #define PROF_SAVE_DATA(file_name)   ::Profiler::instance().dump_csv(file_name);

  typedef std::uint16_t Profiler_id;

// Optional RAII helper macro
  //#define PROF_GUARD(reserve_per_event) ::ProfGuard __prof_guard{reserve_per_event}

#else // PROF_ENABLED == 0  -----------------------------

// No-ops when disabled; zero overhead, zero linking.
  #define PROF_EID(name_literal)    (0u)
  #define PROF_THREAD_INIT()        do{}while(0)
  #define PROF_THREAD_FLUSH()       do{}while(0)
  #define PROF_START(name_lit)      do{}while(0)
  #define PROF_STOP(name_lit)       do{}while(0)
  #define PROF_EVENT(msg_sv)        do{}while(0)
  #define PROF_GUARD(reserve)       do{}while(0)

// Provide a stub class so code can still write `ProfGuard g;` if it wants.
  struct ProfGuard { explicit ProfGuard(std::size_t = 0) {} ~ProfGuard() {} };

#endif

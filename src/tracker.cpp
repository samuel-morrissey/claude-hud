#include "tracker.h"

#include <string.h>

namespace hud {
namespace {

// A comemoração dura pouco; depois a sessão volta a ficar parada.
constexpr uint32_t kDoneMs = 20 * 1000;
// Com tudo parado há este tempo, o boneco dorme.
constexpr uint32_t kSleepMs = 5 * 60 * 1000;
// Sessão sem nenhum evento há este tempo é dada como morta (PC suspenso,
// processo encerrado sem avisar).
constexpr uint32_t kStaleMs = 30 * 60 * 1000;

int priority(State s) {
  switch (s) {
    case State::Waiting:
      return 4;
    case State::Working:
      return 3;
    case State::Thinking:
      return 2;
    case State::Done:
      return 1;
    default:
      return 0;
  }
}

// Copia a próxima palavra de *p para out e avança *p. Devolve false no fim.
bool word(const char** p, char* out, size_t size) {
  const char* s = *p;
  while (*s == ' ' || *s == '\t') s++;
  size_t n = 0;
  while (*s && *s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') {
    if (n + 1 < size) out[n++] = *s;
    s++;
  }
  out[n] = '\0';
  *p = s;
  return n > 0;
}

}  // namespace

Tracker::Session* Tracker::find(Source source, const char* id) {
  for (Session& s : sessions_)
    if (s.used && s.source == source && strcmp(s.id, id) == 0) return &s;
  return nullptr;
}

Tracker::Session* Tracker::allocate(uint32_t now) {
  Session* oldest = &sessions_[0];
  for (Session& s : sessions_) {
    if (!s.used) return &s;
    if (now - s.updated > now - oldest->updated) oldest = &s;
  }
  return oldest;
}

void Tracker::expire(uint32_t now) {
  for (Session& s : sessions_) {
    if (!s.used) continue;
    if (now - s.updated > kStaleMs) {
      s.used = false;
    } else if (s.state == State::Done && now - s.since > kDoneMs) {
      s.state = State::Idle;
      s.since = now;
    }
  }
}

bool Tracker::handle(const char* line, uint32_t now) {
  char source_name[8], state_name[10], id[12];
  if (!word(&line, source_name, sizeof(source_name))) return false;
  if (!word(&line, state_name, sizeof(state_name))) {
    if (strcmp(source_name, "reset") != 0) return false;
    for (Session& s : sessions_) s.used = false;
    return true;
  }
  if (!word(&line, id, sizeof(id))) strcpy(id, "-");

  Source source;
  if (strcmp(source_name, "pc") == 0) source = Source::Pc;
  else if (strcmp(source_name, "cloud") == 0) source = Source::Cloud;
  else if (strcmp(source_name, "phone") == 0) source = Source::Phone;
  else return false;

  Session* s = find(source, id);

  if (strcmp(state_name, "end") == 0) {
    if (s) s->used = false;
    return true;
  }

  State state;
  if (strcmp(state_name, "start") == 0 || strcmp(state_name, "idle") == 0) state = State::Idle;
  else if (strcmp(state_name, "thinking") == 0) state = State::Thinking;
  else if (strcmp(state_name, "working") == 0) state = State::Working;
  else if (strcmp(state_name, "waiting") == 0) state = State::Waiting;
  else if (strcmp(state_name, "done") == 0) state = State::Done;
  else return false;

  if (!s) {
    s = allocate(now);
    *s = Session();
    s->used = true;
    strcpy(s->id, id);
    s->source = source;
    s->state = state;
    s->since = now;
  } else if (s->state != state) {
    s->state = state;
    s->since = now;
  }
  s->updated = now;
  return true;
}

View Tracker::view(uint32_t now) {
  expire(now);

  const Session* best = nullptr;
  uint8_t count = 0;
  for (const Session& s : sessions_) {
    if (!s.used) continue;
    count++;
    if (!best || priority(s.state) > priority(best->state) ||
        (priority(s.state) == priority(best->state) && now - s.since < now - best->since)) {
      best = &s;
    }
  }

  View v{State::Sleep, Source::Pc, count, now, 0, false};
  if (!best) return v;

  v.source = best->source;
  v.since = now - best->since;
  v.state = best->state;
  if (best->state == State::Idle && v.since > kSleepMs) v.state = State::Sleep;
  return v;
}

}  // namespace hud

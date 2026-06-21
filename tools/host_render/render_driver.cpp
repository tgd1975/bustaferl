// Host render harness: drives the real renderFrame() from src/render/layout.cpp
// (compiled against the real Adafruit_GFX + the shimmed Arduino headers in
// shim/) and dumps each state's 1-bpp framebuffer to
// $BUSTAFERL_FRAMES_DIR/<name>.bin for PNG conversion by to_png.py.
//
// This lets us regenerate docs/screenshots/ from the actual firmware renderer
// without an ESP32. See render.sh.

#include "render/layout.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

using namespace bustaferl;

static time_t le(int h, int m) {
  struct tm t {};
  t.tm_year = 2024 - 1900;
  t.tm_mon = 0; // January → CET (+1), no DST, so local HH:MM is exact
  t.tm_mday = 15;
  t.tm_hour = h;
  t.tm_min = m;
  t.tm_isdst = -1;
  return mktime(&t);
}

static Departure busDep(int h, int m) {
  Departure d;
  d.when = le(h, m);
  d.valid = true;
  d.source = DepartureSource::Realtime;
  return d;
}

static Departure sbDep(int h, int m, const char *lbl) {
  Departure d = busDep(h, m);
  std::strncpy(d.line_label, lbl, sizeof(d.line_label) - 1);
  return d;
}

static StreamSnapshot normalSnap() {
  StreamSnapshot s;
  s.api_ok = true;
  s.stream[STREAM_58A_ATZ].slot[0] = busDep(7, 32);
  s.stream[STREAM_58A_ATZ].slot[1] = busDep(7, 47);
  s.stream[STREAM_58A_HIETZING].slot[0] = busDep(7, 35);
  s.stream[STREAM_58A_HIETZING].slot[1] = busDep(7, 50);
  s.stream[STREAM_58B_ATZ].slot[0] = busDep(7, 41);
  s.stream[STREAM_58B_ATZ].slot[1] = busDep(8, 1);
  s.stream[STREAM_SBAHN_HBF].slot[0] = sbDep(7, 33, "S2");
  s.stream[STREAM_SBAHN_HBF].slot[1] = sbDep(7, 39, "S3");
  return s;
}

static void dump(const char *name, const RenderInput &in) {
  Frame fb;
  renderFrame(in, fb);
  const char *dir = std::getenv("BUSTAFERL_FRAMES_DIR");
  if (!dir)
    dir = "/tmp/frames";
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s.bin", dir, name);
  FILE *f = std::fopen(path, "wb");
  if (!f) {
    std::perror(path);
    return;
  }
  std::fwrite(fb.data(), 1, Frame::bytes, f);
  std::fclose(f);
  std::printf("wrote %s\n", path);
}

int main() {
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  {
    RenderInput in{normalSnap(), OverlayKind::None};
    dump("01-normal", in);
  }
  {
    StreamSnapshot s = normalSnap();
    s.stream[STREAM_58B_ATZ].slot[1] = Departure{}; // one missing slot
    RenderInput in{s, OverlayKind::None};
    dump("02-partial-missing", in);
  }
  {
    StreamSnapshot s;
    s.api_ok = true; // responded, but no departures
    RenderInput in{s, OverlayKind::None};
    dump("03-no-data", in);
  }
  {
    RenderInput in{StreamSnapshot{}, OverlayKind::Stale};
    dump("04-stale", in);
  }
  {
    RenderInput in{normalSnap(), OverlayKind::None};
    in.filter_dead_58b = true;
    dump("05-filter-dead", in);
  }
  {
    RenderInput in{StreamSnapshot{}, OverlayKind::StartFailed};
    dump("06-start-failed", in);
  }
  {
    // Evening: bus slots filled from plan hints (visually identical to live);
    // the S-Bahn has no hint path, so it sits empty in the late gap.
    StreamSnapshot s = normalSnap();
    s.stream[STREAM_58A_ATZ].slot[0] = busDep(5, 6);
    s.stream[STREAM_58A_ATZ].slot[1] = busDep(5, 30);
    s.stream[STREAM_58A_HIETZING].slot[0] = busDep(5, 12);
    s.stream[STREAM_58A_HIETZING].slot[1] = busDep(5, 36);
    s.stream[STREAM_58B_ATZ].slot[0] = busDep(5, 20);
    s.stream[STREAM_58B_ATZ].slot[1] = busDep(5, 44);
    for (int i = 0; i < SLOTS_PER_STREAM; ++i) {
      s.stream[STREAM_58A_ATZ].slot[i].source = DepartureSource::Hint;
      s.stream[STREAM_58A_HIETZING].slot[i].source = DepartureSource::Hint;
      s.stream[STREAM_58B_ATZ].slot[i].source = DepartureSource::Hint;
    }
    s.stream[STREAM_SBAHN_HBF].slot[0] = Departure{};
    s.stream[STREAM_SBAHN_HBF].slot[1] = Departure{};
    RenderInput in{s, OverlayKind::None};
    dump("07-evening-hint", in);
  }
  {
    RenderInput in{normalSnap(), OverlayKind::None};
    in.oebb_auth_dead = true;
    dump("08-oebb-auth", in);
  }
  {
    RenderInput in{StreamSnapshot{}, OverlayKind::Boot};
    dump("09-boot", in);
  }
  return 0;
}

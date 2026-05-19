#include "logic/filter_builder.h"

#include "config.h"

namespace bustaferl {

void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
  // STREAM_SBAHN_HBF is filled by snapshot_fetcher::fetchOebbStream (Schritt
  // 5), not by the OGD path. Leave default-constructed — the parser keys off
  // `rbl != 0` and skips this slot.
}

void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {DIVA_TULLNERTALGASSE, LINE_58A, EFA_TOWARDS_58A_ATZ, ""};
  f[STREAM_58A_HIETZING] = {DIVA_TULLNERTALGASSE, LINE_58A,
                            EFA_TOWARDS_58A_HIETZING, ""};
  f[STREAM_58B_ATZ] = {DIVA_ENDEMANNGASSE, LINE_58B, EFA_TOWARDS_58B_ATZ, ""};
  // S-Bahn has no EFA schedule path (Variante 1 — kein Hint-Pfad).
  // Default-leave with diva=0 so schedule_fetcher's diva-skip-guard takes it.
}

} // namespace bustaferl

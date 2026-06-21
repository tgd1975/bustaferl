#include "logic/filter_builder.h"

#include "config.h"

namespace bustaferl {

void buildStreamFilters(StreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {RBL_TULL_ATZGERSDORF, LINE_58A, TOWARDS_58A_ATZ};
  f[STREAM_58A_HIETZING] = {RBL_TULL_HIETZING, LINE_58A, TOWARDS_58A_HIETZING};
  f[STREAM_58B_ATZ] = {RBL_ENDEMANN, LINE_58B, FILTER_TOWARDS_58B};
  // STREAM_SBAHN_HBF left default (rbl = 0): fetched via the ÖBB POST path,
  // never matched by the OGD monitor parser (which has no RBL 0).
}

void buildScheduleFilters(ScheduleStreamFilter (&f)[STREAM_COUNT]) {
  f[STREAM_58A_ATZ] = {DIVA_TULLNERTALGASSE, LINE_58A, EFA_TOWARDS_58A_ATZ, ""};
  f[STREAM_58A_HIETZING] = {DIVA_TULLNERTALGASSE, LINE_58A,
                            EFA_TOWARDS_58A_HIETZING, ""};
  f[STREAM_58B_ATZ] = {DIVA_ENDEMANNGASSE, LINE_58B, EFA_TOWARDS_58B_ATZ, ""};
  // STREAM_SBAHN_HBF left default (diva = 0): schedule_fetcher skips it.
}

OebbStreamFilter buildOebbFilter() {
  OebbStreamFilter f;
  f.stb_eva = EVA_WIEN_ATZGERSDORF;
  f.dir_eva = EVA_WIEN_HBF;
  f.products = OEBB_JNYFLTR_PRODUCTS;
  f.max_jny = OEBB_MAX_JNY;
  return f;
}

} // namespace bustaferl

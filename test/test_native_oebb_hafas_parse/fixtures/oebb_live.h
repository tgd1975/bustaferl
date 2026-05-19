#pragma once

// Captured live responses from the OEBB HAFAS mgate.exe StationBoard
// endpoint on 2026-05-19. stbLoc=Wien Atzgersdorf (1292301),
// dirLoc=Wien Hbf (1290401), products=63, maxJny=6.
//
// Sources: .tmp/poc-oebb/sample-{1,2,3}.json (pre-phase PoC).
// Each capture has 6 jnyL entries, all err=OK, no cancellations.

// first capture 14:12 — 3× S2/S4/S1
static const char *kSample1Json = R"JSON(
{
  "ver": "1.67",
  "lang": "deu",
  "id": "bustaferl",
  "err": "OK",
  "graph": {
    "id": "standard",
    "index": 0
  },
  "subGraph": {
    "id": "global",
    "index": 0
  },
  "view": {
    "id": "standard",
    "index": 0,
    "type": "WGS84"
  },
  "svcResL": [
    {
      "meth": "StationBoard",
      "err": "OK",
      "res": {
        "common": {
          "locL": [
            {
              "lid": "A=1@O=Wien Atzgersdorf Bahnhof@X=16288571@Y=48146953@U=81@L=8100634@",
              "type": "S",
              "name": "Wien Atzgersdorf Bahnhof",
              "icoX": 3,
              "extId": "8100634",
              "state": "F",
              "crd": {
                "x": 16288571,
                "y": 48146953,
                "floor": 0
              },
              "pCls": 48,
              "pRefL": [
                1,
                2
              ],
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8103106",
                  "type": "U"
                },
                {
                  "id": "Lga",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Wolkersdorf im Weinviertel Bahnhof@X=16512681@Y=48378425@U=81@L=8101793@",
              "type": "S",
              "name": "Wolkersdorf im Weinviertel Bahnhof",
              "icoX": 3,
              "extId": "8101793",
              "state": "F",
              "crd": {
                "x": 16512681,
                "y": 48378425,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102869",
                  "type": "U"
                },
                {
                  "id": "Wol",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Absdorf-Hippersdorf Bahnhof@X=15985760@Y=48401995@U=81@L=8100202@",
              "type": "S",
              "name": "Absdorf-Hippersdorf Bahnhof",
              "icoX": 3,
              "extId": "8100202",
              "state": "F",
              "crd": {
                "x": 15985760,
                "y": 48401995,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102423",
                  "type": "U"
                },
                {
                  "id": "Ah",
                  "type": "B"
                }
              ],
              "chgTime": "000300",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Gänserndorf Bahnhof@X=16732746@Y=48340302@U=81@L=8100245@",
              "type": "S",
              "name": "Gänserndorf Bahnhof",
              "icoX": 3,
              "extId": "8100245",
              "state": "F",
              "crd": {
                "x": 16732746,
                "y": 48340302,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102816",
                  "type": "U"
                },
                {
                  "id": "Gae",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Mistelbach/Zaya Bahnhof@X=16563623@Y=48565688@U=81@L=8101245@",
              "type": "S",
              "name": "Mistelbach/Zaya Bahnhof",
              "icoX": 3,
              "extId": "8101245",
              "state": "F",
              "crd": {
                "x": 16563623,
                "y": 48565688,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                },
                {
                  "type": "REM",
                  "remX": 11,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 572260352
                }
              ],
              "globalIdL": [
                {
                  "id": "8102877",
                  "type": "U"
                },
                {
                  "id": "Mb",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Hollabrunn Bahnhof@X=16072290@Y=48562955@U=81@L=8100274@",
              "type": "S",
              "name": "Hollabrunn Bahnhof",
              "icoX": 3,
              "extId": "8100274",
              "state": "F",
              "crd": {
                "x": 16072290,
                "y": 48562955,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102662",
                  "type": "U"
                },
                {
                  "id": "Oh",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Marchegg Bahnhof@X=16917528@Y=48249798@U=81@L=8100466@",
              "type": "S",
              "name": "Marchegg Bahnhof",
              "icoX": 3,
              "extId": "8100466",
              "state": "F",
              "crd": {
                "x": 16917528,
                "y": 48249798,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102899",
                  "type": "U"
                },
                {
                  "id": "Mac",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            }
          ],
          "prodL": [
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28492)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28492",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "name": "",
              "icoX": 3,
              "cls": 16,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "name": "",
              "icoX": 0,
              "cls": 32,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S4:::*",
              "name": "S 4 (Zug-Nr. 28044)",
              "nameS": "S 4",
              "number": "4",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 4     ",
                "num": "28044",
                "line": "4",
                "lineId": "at:obb:vor|S4:",
                "matchId": "4",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_877952"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28850)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28850",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28496)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28496",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S3:::*",
              "name": "S 3 (Zug-Nr. 28268)",
              "nameS": "S 3",
              "number": "3",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 3     ",
                "num": "28268",
                "line": "3",
                "lineId": "at:obb:vor|S3:",
                "matchId": "3",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_837075",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847062",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_860256",
                "HIM_FREETEXT_860257",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_865035",
                "HIM_FREETEXT_865039",
                "HIM_FREETEXT_865041",
                "HIM_FREETEXT_865043",
                "HIM_FREETEXT_865045",
                "HIM_FREETEXT_865047",
                "HIM_FREETEXT_865049",
                "HIM_FREETEXT_865051",
                "HIM_FREETEXT_865053",
                "HIM_FREETEXT_865056",
                "HIM_FREETEXT_865059",
                "HIM_FREETEXT_865060",
                "HIM_FREETEXT_865062",
                "HIM_FREETEXT_865064",
                "HIM_FREETEXT_865067",
                "HIM_FREETEXT_865069",
                "HIM_FREETEXT_865071",
                "HIM_FREETEXT_865074",
                "HIM_FREETEXT_865075",
                "HIM_FREETEXT_865078",
                "HIM_FREETEXT_865080",
                "HIM_FREETEXT_865082",
                "HIM_FREETEXT_865099",
                "HIM_FREETEXT_865101",
                "HIM_FREETEXT_865103",
                "HIM_FREETEXT_865105",
                "HIM_FREETEXT_865107",
                "HIM_FREETEXT_865109",
                "HIM_FREETEXT_865111",
                "HIM_FREETEXT_865113",
                "HIM_FREETEXT_865115",
                "HIM_FREETEXT_865117",
                "HIM_FREETEXT_865120",
                "HIM_FREETEXT_865123",
                "HIM_FREETEXT_865126",
                "HIM_FREETEXT_865132",
                "HIM_FREETEXT_865134",
                "HIM_FREETEXT_865136",
                "HIM_FREETEXT_865138",
                "HIM_FREETEXT_869548",
                "HIM_FREETEXT_869550",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_878090"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28858)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28858",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097"
              ]
            }
          ],
          "opL": [
            {
              "name": "Nahreisezug",
              "icoX": 1,
              "matchId": "Nahreisezug"
            }
          ],
          "remL": [
            {
              "type": "A",
              "code": "gi",
              "prio": 250,
              "icoX": 2,
              "txtN": "Stationsinformation vorhanden"
            },
            {
              "type": "A",
              "code": "OB",
              "prio": 0,
              "icoX": 5,
              "txtN": "Niederflurfahrzeug"
            },
            {
              "type": "A",
              "code": "RO",
              "prio": 150,
              "icoX": 6,
              "txtN": "Rollstuhlstellplatz"
            },
            {
              "type": "A",
              "code": "OA",
              "prio": 150,
              "icoX": 7,
              "txtN": "Rollstuhlstellplatz - Voranmeldung unter +43 5 1717"
            },
            {
              "type": "A",
              "code": "EF",
              "prio": 150,
              "icoX": 8,
              "txtN": "Fahrzeuggebundene Einstiegshilfe"
            },
            {
              "type": "A",
              "code": "OC",
              "prio": 150,
              "icoX": 9,
              "txtN": "rollstuhltaugliches WC"
            },
            {
              "type": "A",
              "code": "FK",
              "prio": 250,
              "icoX": 10,
              "txtN": "Fahrradmitnahme begrenzt möglich"
            },
            {
              "type": "A",
              "code": "K2",
              "prio": 300,
              "icoX": 11,
              "txtN": "nur 2. Klasse"
            },
            {
              "type": "A",
              "code": "SB",
              "prio": 350,
              "icoX": 12,
              "txtN": "Zustieg im Nahverkehr (REX, R, CJX, S-Bahn) nur mit gültiger Fahrkarte"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 13,
              "txtN": "WLAN verfügbar"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 13,
              "txtN": "<b>WLAN verfügbar</b>",
              "rtActivated": true
            },
            {
              "type": "A",
              "code": "gc",
              "prio": 270,
              "icoX": 14,
              "txtN": "Carsharing"
            }
          ],
          "icoL": [
            {
              "res": "prod_comm_t",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "DPN",
              "txt": "Nahreisezug"
            },
            {
              "res": "attr_meta_info"
            },
            {
              "res": "prod_reg",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "rt_cnf"
            },
            {
              "res": "attr_low_floor"
            },
            {
              "res": "attr_wchair"
            },
            {
              "res": "attr_wchair_aviso"
            },
            {
              "res": "attr_wchair_ramp"
            },
            {
              "res": "attr_wchair_wc"
            },
            {
              "res": "attr_bike"
            },
            {
              "res": "attr_2nd"
            },
            {
              "res": "attr_selfservice"
            },
            {
              "res": "attr_wlan"
            },
            {
              "res": "attr_meta_carsharing"
            }
          ],
          "lDrawStyleL": [
            {
              "sIcoX": 0,
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            }
          ],
          "timeStyleL": [
            {
              "mode": "ABS"
            },
            {
              "mode": "DLT",
              "fg": {
                "r": 22,
                "g": 121,
                "b": 85
              }
            },
            {
              "mode": "CNT",
              "icoX": 4
            }
          ]
        },
        "type": "DEP",
        "jnyL": [
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411700#TA#5#DA#190526#1S#8101230#1T#1422#LS#8101793#LT#1526#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1422#TO#8101793#TT#1526#",
            "date": "20260519",
            "prodX": 0,
            "dirTxt": "Wolkersdorf im Weinviertel Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 0,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "143200",
              "dTimeR": "143200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16286665,
              "y": 48143690
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 0,
                "fLocX": 0,
                "tLocX": 1,
                "fIdx": 4,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#410723#TA#1#DA#190526#1S#8100516#1T#1341#LS#8100202#LT#1600#PU#81#RT#1#CA#s#ZE#4#ZB#S 4     #PC#5#FR#8100516#FT#1341#TO#8100202#TT#1600#",
            "date": "20260519",
            "prodX": 3,
            "dirTxt": "Absdorf-Hippersdorf Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 16,
              "dProdX": 3,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "144400",
              "dTimeR": "144400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16295645,
              "y": 48085997
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 3,
                "fLocX": 0,
                "tLocX": 2,
                "fIdx": 16,
                "tIdx": 39
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412335#TA#0#DA#190526#1S#8101150#1T#1457#LS#8100245#LT#1602#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1457#TO#8100245#TT#1602#",
            "date": "20260519",
            "prodX": 4,
            "dirTxt": "Gänserndorf Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 4,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "145900",
              "dTimeR": "145900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 4,
                "fLocX": 0,
                "tLocX": 3,
                "fIdx": 1,
                "tIdx": 20
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411674#TA#3#DA#190526#1S#8101230#1T#1452#LS#8101245#LT#1629#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1452#TO#8101245#TT#1629#",
            "date": "20260519",
            "prodX": 5,
            "dirTxt": "Mistelbach/Zaya Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 5,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "150200",
              "dTimeR": "150200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 5,
                "fLocX": 0,
                "tLocX": 4,
                "fIdx": 4,
                "tIdx": 30
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411263#TA#0#DA#190526#1S#8101163#1T#1428#LS#8100274#LT#1639#PU#81#RT#1#CA#s#ZE#3#ZB#S 3     #PC#5#FR#8101163#FT#1428#TO#8100274#TT#1639#",
            "date": "20260519",
            "prodX": 6,
            "dirTxt": "Hollabrunn Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 11,
              "dProdX": 6,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "151400",
              "dTimeR": "151400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16226428,
              "y": 47953658
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 6,
                "fLocX": 0,
                "tLocX": 5,
                "fIdx": 11,
                "tIdx": 38
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412346#TA#0#DA#190526#1S#8101150#1T#1527#LS#8100466#LT#1645#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1527#TO#8100466#TT#1645#",
            "date": "20260519",
            "prodX": 7,
            "dirTxt": "Marchegg Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 7,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "152900",
              "dTimeR": "152900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 7,
                "fLocX": 0,
                "tLocX": 6,
                "fIdx": 1,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          }
        ],
        "fpB": "20260312",
        "fpE": "20261212",
        "planrtTS": "1779193850",
        "sD": "20260519",
        "sT": "143120",
        "locRefL": [
          0
        ]
      }
    }
  ]
}
)JSON";

// second capture 14:49 — 3× S1/S2/S3
static const char *kSample2Json = R"JSON(
{
  "ver": "1.67",
  "lang": "deu",
  "id": "bustaferl",
  "err": "OK",
  "graph": {
    "id": "standard",
    "index": 0
  },
  "subGraph": {
    "id": "global",
    "index": 0
  },
  "view": {
    "id": "standard",
    "index": 0,
    "type": "WGS84"
  },
  "svcResL": [
    {
      "meth": "StationBoard",
      "err": "OK",
      "res": {
        "common": {
          "locL": [
            {
              "lid": "A=1@O=Wien Atzgersdorf Bahnhof@X=16288571@Y=48146953@U=81@L=8100634@",
              "type": "S",
              "name": "Wien Atzgersdorf Bahnhof",
              "icoX": 3,
              "extId": "8100634",
              "state": "F",
              "crd": {
                "x": 16288571,
                "y": 48146953,
                "floor": 0
              },
              "pCls": 48,
              "pRefL": [
                1,
                2
              ],
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8103106",
                  "type": "U"
                },
                {
                  "id": "Lga",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Gänserndorf Bahnhof@X=16732746@Y=48340302@U=81@L=8100245@",
              "type": "S",
              "name": "Gänserndorf Bahnhof",
              "icoX": 3,
              "extId": "8100245",
              "state": "F",
              "crd": {
                "x": 16732746,
                "y": 48340302,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102816",
                  "type": "U"
                },
                {
                  "id": "Gae",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Mistelbach/Zaya Bahnhof@X=16563623@Y=48565688@U=81@L=8101245@",
              "type": "S",
              "name": "Mistelbach/Zaya Bahnhof",
              "icoX": 3,
              "extId": "8101245",
              "state": "F",
              "crd": {
                "x": 16563623,
                "y": 48565688,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                },
                {
                  "type": "REM",
                  "remX": 9,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 572260352
                }
              ],
              "globalIdL": [
                {
                  "id": "8102877",
                  "type": "U"
                },
                {
                  "id": "Mb",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Hollabrunn Bahnhof@X=16072290@Y=48562955@U=81@L=8100274@",
              "type": "S",
              "name": "Hollabrunn Bahnhof",
              "icoX": 3,
              "extId": "8100274",
              "state": "F",
              "crd": {
                "x": 16072290,
                "y": 48562955,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102662",
                  "type": "U"
                },
                {
                  "id": "Oh",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Marchegg Bahnhof@X=16917528@Y=48249798@U=81@L=8100466@",
              "type": "S",
              "name": "Marchegg Bahnhof",
              "icoX": 3,
              "extId": "8100466",
              "state": "F",
              "crd": {
                "x": 16917528,
                "y": 48249798,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102899",
                  "type": "U"
                },
                {
                  "id": "Mac",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Wolkersdorf im Weinviertel Bahnhof@X=16512681@Y=48378425@U=81@L=8101793@",
              "type": "S",
              "name": "Wolkersdorf im Weinviertel Bahnhof",
              "icoX": 3,
              "extId": "8101793",
              "state": "F",
              "crd": {
                "x": 16512681,
                "y": 48378425,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102869",
                  "type": "U"
                },
                {
                  "id": "Wol",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Hausleiten b.Stockerau Bahnhof@X=16107321@Y=48391297@U=81@L=8100990@",
              "type": "S",
              "name": "Hausleiten b.Stockerau Bahnhof",
              "icoX": 3,
              "extId": "8100990",
              "state": "F",
              "crd": {
                "x": 16107321,
                "y": 48391297,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102462",
                  "type": "U"
                },
                {
                  "id": "Hui",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            }
          ],
          "prodL": [
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28850)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28850",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097",
                "HIM_FREETEXT_878177"
              ]
            },
            {
              "name": "",
              "icoX": 3,
              "cls": 16,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "name": "",
              "icoX": 0,
              "cls": 32,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28496)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28496",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S3:::*",
              "name": "S 3 (Zug-Nr. 28268)",
              "nameS": "S 3",
              "number": "3",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 3     ",
                "num": "28268",
                "line": "3",
                "lineId": "at:obb:vor|S3:",
                "matchId": "3",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_837075",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847062",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_860256",
                "HIM_FREETEXT_860257",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_865035",
                "HIM_FREETEXT_865039",
                "HIM_FREETEXT_865041",
                "HIM_FREETEXT_865043",
                "HIM_FREETEXT_865045",
                "HIM_FREETEXT_865047",
                "HIM_FREETEXT_865049",
                "HIM_FREETEXT_865051",
                "HIM_FREETEXT_865053",
                "HIM_FREETEXT_865056",
                "HIM_FREETEXT_865059",
                "HIM_FREETEXT_865060",
                "HIM_FREETEXT_865062",
                "HIM_FREETEXT_865064",
                "HIM_FREETEXT_865067",
                "HIM_FREETEXT_865069",
                "HIM_FREETEXT_865071",
                "HIM_FREETEXT_865074",
                "HIM_FREETEXT_865075",
                "HIM_FREETEXT_865078",
                "HIM_FREETEXT_865080",
                "HIM_FREETEXT_865082",
                "HIM_FREETEXT_865099",
                "HIM_FREETEXT_865101",
                "HIM_FREETEXT_865103",
                "HIM_FREETEXT_865105",
                "HIM_FREETEXT_865107",
                "HIM_FREETEXT_865109",
                "HIM_FREETEXT_865111",
                "HIM_FREETEXT_865113",
                "HIM_FREETEXT_865115",
                "HIM_FREETEXT_865117",
                "HIM_FREETEXT_865120",
                "HIM_FREETEXT_865123",
                "HIM_FREETEXT_865126",
                "HIM_FREETEXT_865132",
                "HIM_FREETEXT_865134",
                "HIM_FREETEXT_865136",
                "HIM_FREETEXT_865138",
                "HIM_FREETEXT_869548",
                "HIM_FREETEXT_869550",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_878090"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28858)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28858",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097",
                "HIM_FREETEXT_878177"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28504)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28504",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S4:::*",
              "name": "S 4 (Zug-Nr. 28052)",
              "nameS": "S 4",
              "number": "4",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 4     ",
                "num": "28052",
                "line": "4",
                "lineId": "at:obb:vor|S4:",
                "matchId": "4",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_877952"
              ]
            }
          ],
          "opL": [
            {
              "name": "Nahreisezug",
              "icoX": 1,
              "matchId": "Nahreisezug"
            }
          ],
          "remL": [
            {
              "type": "A",
              "code": "gi",
              "prio": 250,
              "icoX": 2,
              "txtN": "Stationsinformation vorhanden"
            },
            {
              "type": "A",
              "code": "OB",
              "prio": 0,
              "icoX": 5,
              "txtN": "Niederflurfahrzeug"
            },
            {
              "type": "A",
              "code": "RO",
              "prio": 150,
              "icoX": 6,
              "txtN": "Rollstuhlstellplatz"
            },
            {
              "type": "A",
              "code": "OA",
              "prio": 150,
              "icoX": 7,
              "txtN": "Rollstuhlstellplatz - Voranmeldung unter +43 5 1717"
            },
            {
              "type": "A",
              "code": "OC",
              "prio": 150,
              "icoX": 8,
              "txtN": "rollstuhltaugliches WC"
            },
            {
              "type": "A",
              "code": "FK",
              "prio": 250,
              "icoX": 9,
              "txtN": "Fahrradmitnahme begrenzt möglich"
            },
            {
              "type": "A",
              "code": "K2",
              "prio": 300,
              "icoX": 10,
              "txtN": "nur 2. Klasse"
            },
            {
              "type": "A",
              "code": "SB",
              "prio": 350,
              "icoX": 11,
              "txtN": "Zustieg im Nahverkehr (REX, R, CJX, S-Bahn) nur mit gültiger Fahrkarte"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 12,
              "txtN": "<b>WLAN verfügbar</b>",
              "rtActivated": true
            },
            {
              "type": "A",
              "code": "gc",
              "prio": 270,
              "icoX": 13,
              "txtN": "Carsharing"
            },
            {
              "type": "A",
              "code": "EF",
              "prio": 150,
              "icoX": 14,
              "txtN": "Fahrzeuggebundene Einstiegshilfe"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 12,
              "txtN": "WLAN verfügbar"
            },
            {
              "type": "A",
              "code": "OG",
              "prio": 150,
              "icoX": 15,
              "txtN": "bedingt rollstuhltaugliches WC"
            }
          ],
          "icoL": [
            {
              "res": "prod_comm_t",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "DPN",
              "txt": "Nahreisezug"
            },
            {
              "res": "attr_meta_info"
            },
            {
              "res": "prod_reg",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "rt_cnf"
            },
            {
              "res": "attr_low_floor"
            },
            {
              "res": "attr_wchair"
            },
            {
              "res": "attr_wchair_aviso"
            },
            {
              "res": "attr_wchair_wc"
            },
            {
              "res": "attr_bike"
            },
            {
              "res": "attr_2nd"
            },
            {
              "res": "attr_selfservice"
            },
            {
              "res": "attr_wlan"
            },
            {
              "res": "attr_meta_carsharing"
            },
            {
              "res": "attr_wchair_ramp"
            },
            {
              "res": "attr_wchair_wc_part"
            }
          ],
          "lDrawStyleL": [
            {
              "sIcoX": 0,
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            }
          ],
          "timeStyleL": [
            {
              "mode": "ABS"
            },
            {
              "mode": "DLT",
              "fg": {
                "r": 22,
                "g": 121,
                "b": 85
              }
            },
            {
              "mode": "CNT",
              "icoX": 4
            }
          ]
        },
        "type": "DEP",
        "jnyL": [
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412335#TA#0#DA#190526#1S#8101150#1T#1457#LS#8100245#LT#1602#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1457#TO#8100245#TT#1602#",
            "date": "20260519",
            "prodX": 0,
            "dirTxt": "Gänserndorf Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 0,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "145900",
              "dTimeR": "145900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 0,
                "fLocX": 0,
                "tLocX": 1,
                "fIdx": 1,
                "tIdx": 20
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411674#TA#3#DA#190526#1S#8101230#1T#1452#LS#8101245#LT#1629#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1452#TO#8101245#TT#1629#",
            "date": "20260519",
            "prodX": 3,
            "dirTxt": "Mistelbach/Zaya Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 3,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "150200",
              "dTimeR": "150200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 3,
                "fLocX": 0,
                "tLocX": 2,
                "fIdx": 4,
                "tIdx": 30
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411263#TA#0#DA#190526#1S#8101163#1T#1428#LS#8100274#LT#1639#PU#81#RT#1#CA#s#ZE#3#ZB#S 3     #PC#5#FR#8101163#FT#1428#TO#8100274#TT#1639#",
            "date": "20260519",
            "prodX": 4,
            "dirTxt": "Hollabrunn Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 11,
              "dProdX": 4,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "151400",
              "dTimeR": "151400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16259005,
              "y": 48018218
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 4,
                "fLocX": 0,
                "tLocX": 3,
                "fIdx": 11,
                "tIdx": 38
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412346#TA#0#DA#190526#1S#8101150#1T#1527#LS#8100466#LT#1645#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1527#TO#8100466#TT#1645#",
            "date": "20260519",
            "prodX": 5,
            "dirTxt": "Marchegg Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 5,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "152900",
              "dTimeR": "152900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 5,
                "fLocX": 0,
                "tLocX": 4,
                "fIdx": 1,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411700#TA#6#DA#190526#1S#8101230#1T#1522#LS#8101793#LT#1626#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1522#TO#8101793#TT#1626#",
            "date": "20260519",
            "prodX": 6,
            "dirTxt": "Wolkersdorf im Weinviertel Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 6,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "153200",
              "dTimeR": "153200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 6,
                "fLocX": 0,
                "tLocX": 5,
                "fIdx": 4,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#410767#TA#0#DA#190526#1S#8100516#1T#1441#LS#8100990#LT#1651#PU#81#RT#1#CA#s#ZE#4#ZB#S 4     #PC#5#FR#8100516#FT#1441#TO#8100990#TT#1651#",
            "date": "20260519",
            "prodX": 7,
            "dirTxt": "Hausleiten b.Stockerau Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 16,
              "dProdX": 7,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "154400",
              "dTimeR": "154400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16238842,
              "y": 47834982
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 1,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 12,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 7,
                "fLocX": 0,
                "tLocX": 6,
                "fIdx": 16,
                "tIdx": 37
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          }
        ],
        "fpB": "20260312",
        "fpE": "20261212",
        "planrtTS": "1779194750",
        "sD": "20260519",
        "sT": "144620",
        "locRefL": [
          0
        ]
      }
    }
  ]
}
)JSON";

// third capture 15:13 — 3× S2/S3/S1
static const char *kSample3Json = R"JSON(
{
  "ver": "1.67",
  "lang": "deu",
  "id": "bustaferl",
  "err": "OK",
  "graph": {
    "id": "standard",
    "index": 0
  },
  "subGraph": {
    "id": "global",
    "index": 0
  },
  "view": {
    "id": "standard",
    "index": 0,
    "type": "WGS84"
  },
  "svcResL": [
    {
      "meth": "StationBoard",
      "err": "OK",
      "res": {
        "common": {
          "locL": [
            {
              "lid": "A=1@O=Wien Atzgersdorf Bahnhof@X=16288571@Y=48146953@U=81@L=8100634@",
              "type": "S",
              "name": "Wien Atzgersdorf Bahnhof",
              "icoX": 3,
              "extId": "8100634",
              "state": "F",
              "crd": {
                "x": 16288571,
                "y": 48146953,
                "floor": 0
              },
              "pCls": 48,
              "pRefL": [
                1,
                2
              ],
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8103106",
                  "type": "U"
                },
                {
                  "id": "Lga",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Mistelbach/Zaya Bahnhof@X=16563623@Y=48565688@U=81@L=8101245@",
              "type": "S",
              "name": "Mistelbach/Zaya Bahnhof",
              "icoX": 3,
              "extId": "8101245",
              "state": "F",
              "crd": {
                "x": 16563623,
                "y": 48565688,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                },
                {
                  "type": "REM",
                  "remX": 1,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 572260352
                }
              ],
              "globalIdL": [
                {
                  "id": "8102877",
                  "type": "U"
                },
                {
                  "id": "Mb",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Hollabrunn Bahnhof@X=16072290@Y=48562955@U=81@L=8100274@",
              "type": "S",
              "name": "Hollabrunn Bahnhof",
              "icoX": 3,
              "extId": "8100274",
              "state": "F",
              "crd": {
                "x": 16072290,
                "y": 48562955,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102662",
                  "type": "U"
                },
                {
                  "id": "Oh",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Marchegg Bahnhof@X=16917528@Y=48249798@U=81@L=8100466@",
              "type": "S",
              "name": "Marchegg Bahnhof",
              "icoX": 3,
              "extId": "8100466",
              "state": "F",
              "crd": {
                "x": 16917528,
                "y": 48249798,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102899",
                  "type": "U"
                },
                {
                  "id": "Mac",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Wolkersdorf im Weinviertel Bahnhof@X=16512681@Y=48378425@U=81@L=8101793@",
              "type": "S",
              "name": "Wolkersdorf im Weinviertel Bahnhof",
              "icoX": 3,
              "extId": "8101793",
              "state": "F",
              "crd": {
                "x": 16512681,
                "y": 48378425,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102869",
                  "type": "U"
                },
                {
                  "id": "Wol",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Hausleiten b.Stockerau Bahnhof@X=16107321@Y=48391297@U=81@L=8100990@",
              "type": "S",
              "name": "Hausleiten b.Stockerau Bahnhof",
              "icoX": 3,
              "extId": "8100990",
              "state": "F",
              "crd": {
                "x": 16107321,
                "y": 48391297,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102462",
                  "type": "U"
                },
                {
                  "id": "Hui",
                  "type": "B"
                }
              ],
              "chgTime": "000500",
              "countryCodeL": [
                "at"
              ]
            },
            {
              "lid": "A=1@O=Gänserndorf Bahnhof@X=16732746@Y=48340302@U=81@L=8100245@",
              "type": "S",
              "name": "Gänserndorf Bahnhof",
              "icoX": 3,
              "extId": "8100245",
              "state": "F",
              "crd": {
                "x": 16732746,
                "y": 48340302,
                "floor": 0
              },
              "pCls": 48,
              "entry": true,
              "msgL": [
                {
                  "type": "REM",
                  "remX": 0,
                  "sty": "I",
                  "tagL": [
                    "RES_LOC_H3"
                  ],
                  "sort": 569638912
                }
              ],
              "globalIdL": [
                {
                  "id": "8102816",
                  "type": "U"
                },
                {
                  "id": "Gae",
                  "type": "B"
                }
              ],
              "chgTime": "000400",
              "countryCodeL": [
                "at"
              ]
            }
          ],
          "prodL": [
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28496)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28496",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "name": "",
              "icoX": 3,
              "cls": 16,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "name": "",
              "icoX": 0,
              "cls": 32,
              "prodCtx": {
                "name": ""
              }
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S3:::*",
              "name": "S 3 (Zug-Nr. 28268)",
              "nameS": "S 3",
              "number": "3",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 3     ",
                "num": "28268",
                "line": "3",
                "lineId": "at:obb:vor|S3:",
                "matchId": "3",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_837075",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847062",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_860256",
                "HIM_FREETEXT_860257",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_865035",
                "HIM_FREETEXT_865039",
                "HIM_FREETEXT_865041",
                "HIM_FREETEXT_865043",
                "HIM_FREETEXT_865045",
                "HIM_FREETEXT_865047",
                "HIM_FREETEXT_865049",
                "HIM_FREETEXT_865051",
                "HIM_FREETEXT_865053",
                "HIM_FREETEXT_865056",
                "HIM_FREETEXT_865059",
                "HIM_FREETEXT_865060",
                "HIM_FREETEXT_865062",
                "HIM_FREETEXT_865064",
                "HIM_FREETEXT_865067",
                "HIM_FREETEXT_865069",
                "HIM_FREETEXT_865071",
                "HIM_FREETEXT_865074",
                "HIM_FREETEXT_865075",
                "HIM_FREETEXT_865078",
                "HIM_FREETEXT_865080",
                "HIM_FREETEXT_865082",
                "HIM_FREETEXT_865099",
                "HIM_FREETEXT_865101",
                "HIM_FREETEXT_865103",
                "HIM_FREETEXT_865105",
                "HIM_FREETEXT_865107",
                "HIM_FREETEXT_865109",
                "HIM_FREETEXT_865111",
                "HIM_FREETEXT_865113",
                "HIM_FREETEXT_865115",
                "HIM_FREETEXT_865117",
                "HIM_FREETEXT_865120",
                "HIM_FREETEXT_865123",
                "HIM_FREETEXT_865126",
                "HIM_FREETEXT_865132",
                "HIM_FREETEXT_865134",
                "HIM_FREETEXT_865136",
                "HIM_FREETEXT_865138",
                "HIM_FREETEXT_869548",
                "HIM_FREETEXT_869550",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_878090",
                "HIM_FREETEXT_878193"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28858)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28858",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097",
                "HIM_FREETEXT_878177"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S2:::*",
              "name": "S 2 (Zug-Nr. 28504)",
              "nameS": "S 2",
              "number": "2",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 2     ",
                "num": "28504",
                "line": "2",
                "lineId": "at:obb:vor|S2:",
                "matchId": "2",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_831981",
                "HIM_FREETEXT_831982",
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_836906",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852678",
                "HIM_FREETEXT_852679",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_856585",
                "HIM_FREETEXT_856586",
                "HIM_FREETEXT_856588",
                "HIM_FREETEXT_856589",
                "HIM_FREETEXT_856590",
                "HIM_FREETEXT_856591",
                "HIM_FREETEXT_856593",
                "HIM_FREETEXT_856594",
                "HIM_FREETEXT_856596",
                "HIM_FREETEXT_856597",
                "HIM_FREETEXT_856599",
                "HIM_FREETEXT_856600",
                "HIM_FREETEXT_856602",
                "HIM_FREETEXT_856603",
                "HIM_FREETEXT_856605",
                "HIM_FREETEXT_856606",
                "HIM_FREETEXT_856609",
                "HIM_FREETEXT_856610",
                "HIM_FREETEXT_856613",
                "HIM_FREETEXT_856615",
                "HIM_FREETEXT_856622",
                "HIM_FREETEXT_856623",
                "HIM_FREETEXT_856625",
                "HIM_FREETEXT_856626",
                "HIM_FREETEXT_856628",
                "HIM_FREETEXT_856629",
                "HIM_FREETEXT_856631",
                "HIM_FREETEXT_856632",
                "HIM_FREETEXT_856634",
                "HIM_FREETEXT_856635",
                "HIM_FREETEXT_856637",
                "HIM_FREETEXT_856638",
                "HIM_FREETEXT_856641",
                "HIM_FREETEXT_856642",
                "HIM_FREETEXT_856644",
                "HIM_FREETEXT_856645",
                "HIM_FREETEXT_856648",
                "HIM_FREETEXT_856651",
                "HIM_FREETEXT_856653",
                "HIM_FREETEXT_856680",
                "HIM_FREETEXT_861846"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S4:::*",
              "name": "S 4 (Zug-Nr. 28052)",
              "nameS": "S 4",
              "number": "4",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 4     ",
                "num": "28052",
                "line": "4",
                "lineId": "at:obb:vor|S4:",
                "matchId": "4",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_877952"
              ]
            },
            {
              "pid": "L::5::S::B1146449194::at:obb:vor|S1:::*",
              "name": "S 1 (Zug-Nr. 28866)",
              "nameS": "S 1",
              "number": "1",
              "icoX": 0,
              "cls": 32,
              "oprX": 0,
              "prodCtx": {
                "name": "S 1     ",
                "num": "28866",
                "line": "1",
                "lineId": "at:obb:vor|S1:",
                "matchId": "1",
                "catOut": "S       ",
                "catOutS": "s",
                "catOutL": "S-Bahn",
                "catIn": "s",
                "catCode": "5",
                "admin": "81____"
              },
              "himIdL": [
                "HIM_FREETEXT_834855",
                "HIM_FREETEXT_835981",
                "HIM_FREETEXT_846442",
                "HIM_FREETEXT_846446",
                "HIM_FREETEXT_847863",
                "HIM_FREETEXT_847864",
                "HIM_FREETEXT_852534",
                "HIM_FREETEXT_852582",
                "HIM_FREETEXT_852897",
                "HIM_FREETEXT_861846",
                "HIM_FREETEXT_868166",
                "HIM_FREETEXT_868169",
                "HIM_FREETEXT_876228",
                "HIM_FREETEXT_877952",
                "HIM_FREETEXT_877966",
                "HIM_FREETEXT_878076",
                "HIM_FREETEXT_878097",
                "HIM_FREETEXT_878177"
              ]
            }
          ],
          "opL": [
            {
              "name": "Nahreisezug",
              "icoX": 1,
              "matchId": "Nahreisezug"
            }
          ],
          "remL": [
            {
              "type": "A",
              "code": "gi",
              "prio": 250,
              "icoX": 2,
              "txtN": "Stationsinformation vorhanden"
            },
            {
              "type": "A",
              "code": "gc",
              "prio": 270,
              "icoX": 5,
              "txtN": "Carsharing"
            },
            {
              "type": "A",
              "code": "OB",
              "prio": 0,
              "icoX": 6,
              "txtN": "Niederflurfahrzeug"
            },
            {
              "type": "A",
              "code": "RO",
              "prio": 150,
              "icoX": 7,
              "txtN": "Rollstuhlstellplatz"
            },
            {
              "type": "A",
              "code": "OA",
              "prio": 150,
              "icoX": 8,
              "txtN": "Rollstuhlstellplatz - Voranmeldung unter +43 5 1717"
            },
            {
              "type": "A",
              "code": "EF",
              "prio": 150,
              "icoX": 9,
              "txtN": "Fahrzeuggebundene Einstiegshilfe"
            },
            {
              "type": "A",
              "code": "OC",
              "prio": 150,
              "icoX": 10,
              "txtN": "rollstuhltaugliches WC"
            },
            {
              "type": "A",
              "code": "FK",
              "prio": 250,
              "icoX": 11,
              "txtN": "Fahrradmitnahme begrenzt möglich"
            },
            {
              "type": "A",
              "code": "K2",
              "prio": 300,
              "icoX": 12,
              "txtN": "nur 2. Klasse"
            },
            {
              "type": "A",
              "code": "SB",
              "prio": 350,
              "icoX": 13,
              "txtN": "Zustieg im Nahverkehr (REX, R, CJX, S-Bahn) nur mit gültiger Fahrkarte"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 14,
              "txtN": "WLAN verfügbar"
            },
            {
              "type": "A",
              "code": "WV",
              "prio": 710,
              "icoX": 14,
              "txtN": "<b>WLAN verfügbar</b>",
              "rtActivated": true
            },
            {
              "type": "A",
              "code": "OG",
              "prio": 150,
              "icoX": 15,
              "txtN": "bedingt rollstuhltaugliches WC"
            }
          ],
          "icoL": [
            {
              "res": "prod_comm_t",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "DPN",
              "txt": "Nahreisezug"
            },
            {
              "res": "attr_meta_info"
            },
            {
              "res": "prod_reg",
              "fg": {
                "r": 255,
                "g": 255,
                "b": 255
              },
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "res": "rt_cnf"
            },
            {
              "res": "attr_meta_carsharing"
            },
            {
              "res": "attr_low_floor"
            },
            {
              "res": "attr_wchair"
            },
            {
              "res": "attr_wchair_aviso"
            },
            {
              "res": "attr_wchair_ramp"
            },
            {
              "res": "attr_wchair_wc"
            },
            {
              "res": "attr_bike"
            },
            {
              "res": "attr_2nd"
            },
            {
              "res": "attr_selfservice"
            },
            {
              "res": "attr_wlan"
            },
            {
              "res": "attr_wchair_wc_part"
            }
          ],
          "lDrawStyleL": [
            {
              "sIcoX": 0,
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            },
            {
              "type": "SOLID",
              "bg": {
                "r": 0,
                "g": 96,
                "b": 240
              }
            }
          ],
          "timeStyleL": [
            {
              "mode": "ABS"
            },
            {
              "mode": "DLT",
              "fg": {
                "r": 22,
                "g": 121,
                "b": 85
              }
            },
            {
              "mode": "CNT",
              "icoX": 4
            }
          ]
        },
        "type": "DEP",
        "jnyL": [
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411674#TA#3#DA#190526#1S#8101230#1T#1452#LS#8101245#LT#1629#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1452#TO#8101245#TT#1629#",
            "date": "20260519",
            "prodX": 0,
            "dirTxt": "Mistelbach/Zaya Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 0,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "150200",
              "dTimeR": "150200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16286836,
              "y": 48144031
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 1,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 0,
                "fLocX": 0,
                "tLocX": 1,
                "fIdx": 4,
                "tIdx": 30
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411263#TA#0#DA#190526#1S#8101163#1T#1428#LS#8100274#LT#1639#PU#81#RT#1#CA#s#ZE#3#ZB#S 3     #PC#5#FR#8101163#FT#1428#TO#8100274#TT#1639#",
            "date": "20260519",
            "prodX": 3,
            "dirTxt": "Hollabrunn Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 11,
              "dProdX": 3,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "151400",
              "dTimeR": "151400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16295645,
              "y": 48085997
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 2,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 3,
                "fLocX": 0,
                "tLocX": 2,
                "fIdx": 11,
                "tIdx": 38
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412346#TA#0#DA#190526#1S#8101150#1T#1527#LS#8100466#LT#1645#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1527#TO#8100466#TT#1645#",
            "date": "20260519",
            "prodX": 4,
            "dirTxt": "Marchegg Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 4,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "152900",
              "dTimeR": "152900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 3,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 4,
                "fLocX": 0,
                "tLocX": 3,
                "fIdx": 1,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#411700#TA#6#DA#190526#1S#8101230#1T#1522#LS#8101793#LT#1626#PU#81#RT#1#CA#s#ZE#2#ZB#S 2     #PC#5#FR#8101230#FT#1522#TO#8101793#TT#1626#",
            "date": "20260519",
            "prodX": 5,
            "dirTxt": "Wolkersdorf im Weinviertel Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 4,
              "dProdX": 5,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "153200",
              "dTimeR": "153200",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 5,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 4,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 5,
                "fLocX": 0,
                "tLocX": 4,
                "fIdx": 4,
                "tIdx": 22
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#410767#TA#0#DA#190526#1S#8100516#1T#1441#LS#8100990#LT#1651#PU#81#RT#1#CA#s#ZE#4#ZB#S 4     #PC#5#FR#8100516#FT#1441#TO#8100990#TT#1651#",
            "date": "20260519",
            "prodX": 6,
            "dirTxt": "Hausleiten b.Stockerau Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 16,
              "dProdX": 6,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "154400",
              "dTimeR": "154400",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "pos": {
              "x": 16225511,
              "y": 47959608
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 12,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 10,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 5,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 6,
                "fLocX": 0,
                "tLocX": 5,
                "fIdx": 16,
                "tIdx": 37
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          },
          {
            "jid": "2|#VN#1#ST#1778665214#PI#0#ZI#412295#TA#1#DA#190526#1S#8101150#1T#1557#LS#8100245#LT#1702#PU#81#RT#1#CA#s#ZE#1#ZB#S 1     #PC#5#FR#8101150#FT#1557#TO#8100245#TT#1702#",
            "date": "20260519",
            "prodX": 7,
            "dirTxt": "Gänserndorf Bahnhof",
            "dirFlg": "x",
            "status": "P",
            "isRchbl": true,
            "stbStop": {
              "locX": 0,
              "idx": 1,
              "dProdX": 7,
              "dPltfS": {
                "type": "PL",
                "txt": "1"
              },
              "dPltfR": {
                "type": "PL",
                "txt": "1"
              },
              "dTimeS": "155900",
              "dTimeR": "155900",
              "dTimeFS": {
                "styleX": 0
              },
              "dTimeFR": {
                "styleX": 1,
                "txtA": "pünktlich"
              },
              "dTimeFC": {
                "styleX": 2
              },
              "dProgType": "PROGNOSED",
              "dTZOffset": 120,
              "type": "N"
            },
            "msgL": [
              {
                "type": "REM",
                "remX": 2,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "SUM_CON_FTR_H3",
                  "RES_JNY_H3"
                ],
                "sort": 268435456
              },
              {
                "type": "REM",
                "remX": 3,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 4,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 6,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 556531712
              },
              {
                "type": "REM",
                "remX": 7,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 569638912
              },
              {
                "type": "REM",
                "remX": 8,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 576192512
              },
              {
                "type": "REM",
                "remX": 9,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 582746112
              },
              {
                "type": "REM",
                "remX": 11,
                "sty": "I",
                "fLocX": 0,
                "tLocX": 6,
                "tagL": [
                  "RES_JNY_DTL"
                ],
                "sort": 629932032
              }
            ],
            "subscr": "F",
            "prodL": [
              {
                "prodX": 7,
                "fLocX": 0,
                "tLocX": 6,
                "fIdx": 1,
                "tIdx": 20
              }
            ],
            "sumLDrawStyleX": 0,
            "resLDrawStyleX": 1,
            "trainStartDate": "20260519"
          }
        ],
        "fpB": "20260312",
        "fpE": "20261212",
        "planrtTS": "1779195650",
        "sD": "20260519",
        "sT": "150123",
        "locRefL": [
          0
        ]
      }
    }
  ]
}
)JSON";
